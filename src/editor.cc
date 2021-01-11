#define _POSIX_C_SOURCE 200809L

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <unistd.h>
#include "editor.h"
#include "screen.h"

namespace fs = std::filesystem;

constexpr const char* KILO_VERSION = "0.0.1";

constexpr const int KILO_QUIT_TIMES = 3;

#define CTRL_KEY(k) ((k) & 0x1f)

bool is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

FGColor syntaxToColor(HL hl) {
  switch (hl) {
    case HL::COMMENT:
    case HL::MLCOMMENT: return FGColor::CYAN;
    case HL::KEYWORD1: return FGColor::YELLOW;
    case HL::KEYWORD2: return FGColor::GREEN;
    case HL::STRING: return FGColor::MAGENTA;
    case HL::NUMBER: return FGColor::RED;
    case HL::MATCH: return FGColor::BLUE;
    default: return FGColor::WHITE;
  }
}
Editor::Editor() : cx{0}, cy{0}, rx{0}, rowoff{0}, coloff{0},
rows{}, dirty{false}, filename{}, statusmsg{0}, statusmsg_time{0},
syntax{std::nullopt}, hldb {
  {
    "c",
    { ".c", ".h", ".cc", ".cpp" },
    {
      "switch", "if", "while", "for", "break", "continue", "return", "else",
      "struct", "union", "typedef", "static", "enum", "class", "case",
      "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
      "void|"
    },
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  }
} {
}

void Editor::delChar() {
  if (cy == rows.size()) {
    return;
  }
  if (cx == 0 && cy == 0) {
    return;
  }

  Row& row = rows[cy];
  if (cx > 0) {
    row.erase(cx - 1);
    updateSyntax(row);
    cx--;
  } else {
    cx = rows[cy - 1].chars.length();
    rows[cy - 1].append(row.chars);
    updateSyntax(rows[cy - 1]);
    delRow(cy);
    cy--;
  }
  dirty = true;
}

void Editor::delRow(std::size_t at) {
  if (at >= rows.size()) {
    return;
  }
  rows.erase(rows.begin() + at);
  dirty = true;
}

void Editor::drawMessageBar(Screen& screen) {
  screen.clearToEOL();
  int msglen = strlen(statusmsg);
  if (msglen > screen.cols) {
    msglen = screen.cols;
  }
  if (msglen && time(NULL) - statusmsg_time < 5) {
    screen.print(statusmsg, msglen);
  }
}

void Editor::drawRows(Screen& screen) {
  for (auto y = 0; y < screen.rows; y++) {
    std::size_t filerow = y + rowoff;
    if (filerow >= rows.size()) {
      if (rows.size() == 0 && y == screen.rows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > screen.cols) {
          welcomelen = screen.cols;
        }
        int padding = (screen.cols - welcomelen) / 2;
        if (padding) {
          screen.printChar('~');
          padding--;
        }
        while (padding--) {
          screen.printChar(' ');
        }
        screen.print(welcome, welcomelen);
      } else {
        screen.printChar('~');
      }
    } else {
      int len = rows[filerow].render.length() - coloff;
      if (len < 0) {
        len = 0;
      }
      if (len > screen.cols) {
        len = screen.cols;
      }
      auto render = rows[filerow].render;
      auto hl = rows[filerow].hl;
      FGColor current_color = FGColor::RESET;
      for (auto j = 0; j < len; j++) {
        if (iscntrl(render[coloff + j])) {
          char sym = (render[coloff + j] <= 26) ? '@' + render[coloff + j] : '?';
          screen.inverse();
          screen.printChar(sym);
          screen.inverse(false);
          if (current_color != FGColor::RESET) {
            screen.setFGColor(current_color);
          }
        } else if (hl[coloff + j] == HL::NORMAL) {
          if (current_color != FGColor::RESET) {
            screen.setFGColor(FGColor::RESET);
            current_color = FGColor::RESET;
          }
          screen.printChar(render[coloff + j]);
        } else {
          FGColor color = syntaxToColor(hl[coloff + j]);
          if (color != current_color) {
            current_color = color;
            screen.setFGColor(color);
          }
          screen.printChar(render[coloff + j]);
        }
      }
      screen.setFGColor(FGColor::RESET);
    }

    screen.clearToEOL();
    screen.print("\r\n", 2);
  }
}

void Editor::drawStatusBar(Screen& screen) {
  screen.inverse();
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %ld lines %s",
    filename.empty() ? "[No Name]" : filename.c_str(), rows.size(),
    dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %ld/%ld",
    syntax ? syntax->filetype.c_str() : "no ft", cy + 1, rows.size());
  if (len > screen.cols) {
    len = screen.cols;
  }
  screen.print(status, len);
  while (len < screen.cols) {
    if (screen.cols - len == rlen) {
      screen.print(rstatus, rlen);
      break;
    } else {
      screen.printChar(' ');
      len++;
    }
  }
  screen.inverse(false);
  screen.print("\r\n", 2);
}

void Editor::draw(Screen& screen) {
  scroll(screen);

  screen.hideCursor();
  screen.moveCursor(0, 0);

  drawRows(screen);
  drawStatusBar(screen);
  drawMessageBar(screen);

  screen.moveCursor((cy - rowoff) + 1, (rx - coloff) + 1);
  screen.showCursor();

  screen.refresh();
}

void Editor::find(Screen& screen) {
  int saved_cx = cx;
  int saved_cy = cy;
  int saved_coloff = coloff;
  int saved_rowoff = rowoff;

  std::string query = prompt(screen, "Search: %s (Use ESC/Arrows/Enter)",
    std::make_optional(&Editor::findCallback));

  if (query.empty()) {
    cx = saved_cx;
    cy = saved_cy;
    coloff = saved_coloff;
    rowoff = saved_rowoff;
  }
}

void Editor::findCallback(std::string& query, int key) {
  static std::optional<std::size_t> last_match = std::nullopt;
  static int direction = 1;

  static int saved_hl_line;
  static Highlight saved_hl;

  if (!saved_hl.empty()) {
    rows[saved_hl_line].hl = saved_hl;
    saved_hl.clear();
  }

  if (key == '\r' || key == '\x1b') {
    last_match = std::nullopt;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = std::nullopt;
    direction = 1;
  }

  if (last_match == std::nullopt) {
    direction = 1;
  }
  std::optional<std::size_t> current = last_match;
  for (std::size_t i = 0; i < rows.size(); i++) {
    *current += direction;
    if (current == std::nullopt) {
      current = rows.size() - 1;
    } else if (*current == rows.size()) {
      current = 0;
    }

    Row& row = rows[*current];
    auto match = row.render.find(query);
    if (match != std::string::npos) {
      last_match = *current;
      cy = *current;
      cx = row.rxtocx(match);
      rowoff = rows.size();

      saved_hl_line = *current;
      saved_hl = row.hl;
      std::fill(row.hl.begin() + match,
        row.hl.begin() + match + query.length(), HL::MATCH);
      break;
    }
  }
}

void Editor::insertChar(int c) {
  if (cy == rows.size()) {
    insertRow(rows.size(), "");
  }
  rows[cy].insert(cx, c);
  updateSyntax(rows[cy]);
  dirty = true;
  cx++;
}

void Editor::insertNewline() {
  if (cx == 0) {
    insertRow(cy, "");
  } else {
    auto copy = rows[cy].chars.substr(cx);
    insertRow(cy + 1, copy.c_str());
    updateSyntax(rows[cy + 1]);
    rows[cy].chars.erase(cx);
    rows[cy].update();
    updateSyntax(rows[cy]);
  }
  cy++;
  cx = 0;
}

void Editor::insertRow(std::size_t at, const char* s) {
  if (at > rows.size()) {
    return;
  }

  rows.emplace(rows.begin() + at, at, s);
  for (auto j = at + 1; j < rows.size(); j++) {
    rows[j].idx++;
  }

  rows[at].update();

  dirty = true;
}

void Editor::moveCursor(int key) {
  std::optional<std::reference_wrapper<Row>> row = (cy >= rows.size())
    ? std::nullopt
    : std::make_optional(std::ref(rows[cy]));

  switch (key) {
    case ARROW_LEFT:
      if (cx != 0) {
        cx--;
      } else if (cy > 0) {
        cy--;
        cx = rows[cy].chars.length();
      }
      break;
    case ARROW_RIGHT:
      if (row && cx < row->get().chars.length()) {
        cx++;
      } else if (row && cx == row->get().chars.length()) {
        cy++;
        cx = 0;
      }
      break;
    case ARROW_UP:
      if (cy != 0) {
        cy--;
      }
      break;
    case ARROW_DOWN:
      if (cy < rows.size()) {
        cy++;
      }
      break;
  }

  row = (cy >= rows.size())
    ? std::nullopt
    : std::make_optional(std::ref(rows[cy]));
  std::size_t rowlen = row ? row->get().chars.length() : 0;
  if (cx > rowlen) {
    cx = rowlen;
  }
}

void Editor::openFile(Screen& screen, const char *fn) {
  filename = fn;

  selectSyntaxHighlight();

  FILE *fp = fopen(fn, "r");
  if (!fp) {
    screen.die("fopen");
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r')) {
      linelen--;
    }
    insertRow(rows.size(), line);
    rows[rows.size() - 1].erase(linelen);
    updateSyntax(rows[rows.size() - 1]);
  }
  free(line);
  fclose(fp);
  dirty = false;
}

void Editor::processKeypress(Screen& screen) {
  static int quit_times = KILO_QUIT_TIMES;

  int c = screen.readKey();

  switch (c) {
    case '\r':
      insertNewline();
      break;

    case CTRL_KEY('q'):
      if (dirty && quit_times > 0) {
        setStatusMessage("WARNING!!! File has unsaved changes. "
          "Press Ctrl-Q %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      if (!screen.clear()) {
        screen.die("write");
      }
      exit(0);
      break;

    case CTRL_KEY('s'):
      saveFile(screen);
      break;

    case HOME_KEY:
      cx = 0;
      break;

    case END_KEY:
      if (cy < rows.size()) {
        cx = rows[cy].chars.length();
      }
      break;

    case CTRL_KEY('f'):
      find(screen);
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) {
        moveCursor(ARROW_RIGHT);
      }
      delChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          cy = rowoff;
        } else if (c == PAGE_DOWN) {
          cy = rowoff + screen.rows - 1;
          if (cy > rows.size()) {
            cy = rows.size();
          }
        }

        int times = screen.rows;
        while (times--) {
          moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      moveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      insertChar(c);
      break;
  }

  quit_times = KILO_QUIT_TIMES;
}

std::string Editor::prompt(Screen& screen, const char* msg,
std::optional<std::function<void(Editor*, std::string&, int)>> callback) {
  std::string buf;

  while (true) {
    setStatusMessage(msg, buf.c_str());
    draw(screen);

    int c = screen.readKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
    }
    else if (c == '\x1b') {
      setStatusMessage("");
      if (callback) {
        std::invoke(*callback, this, buf, c);
      }
      return "";
    } else if (c == '\r') {
      if (!buf.empty()) {
        setStatusMessage("");
        if (callback) {
          std::invoke(*callback, this, buf, c);
        }
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      buf += c;
    }

    if (callback) {
      std::invoke(*callback, this, buf, c);
    }
  }
}

void Editor::saveFile(Screen& screen) {
  if (filename.empty()) {
    filename = prompt(screen, "Save as: %s (ESC to cancel)", std::nullopt);
    if (filename.empty()) {
      setStatusMessage("Save aborted");
      return;
    }
    selectSyntaxHighlight();
  }

  std::string buf;

  for (auto& row: rows) {
    buf += row.chars + '\n';
  }

  try {
    std::ofstream file(filename.native());
    file.exceptions(std::ofstream::failbit);
    fs::permissions(filename, fs::perms::owner_read | fs::perms::owner_write |
      fs::perms::group_read | fs::perms::others_read);
    fs::resize_file(filename, buf.length());
    file.write(buf.data(), buf.length());
    dirty = false;
    setStatusMessage("%ld bytes written to disk", buf.length());
  } catch (std::system_error& e) {
    setStatusMessage("Can't save! %s", e.what());
    filename.clear();
  }
}

void Editor::scroll(Screen& screen) {
  rx = 0;

  if (cy < rows.size()) {
    rx = rows[cy].cxtorx(cx);
  }

  if (cy < rowoff) {
    rowoff = cy;
  }
  if (cy >= rowoff + screen.rows) {
    rowoff = cy - screen.rows + 1;
  }
  if (rx < coloff) {
    coloff = rx;
  }
  if (rx >= coloff + screen.cols) {
    coloff = rx - screen.cols + 1;
  }
}

void Editor::selectSyntaxHighlight() {
  syntax = std::nullopt;

  if (filename.empty()) {
    return;
  }

  auto ext = filename.extension();
  auto fn = filename.filename().native();

  for (auto& hl: hldb) {
    for (auto& match: hl.filematch) {
      auto is_ext = (match[0] == '.');
      if ((is_ext && ext != fn && ext == match) ||
      (!is_ext && fn.find(match) != std::string::npos)) {
        syntax = hl;

        for (std::size_t filerow = 0; filerow < rows.size(); filerow++) {
          updateSyntax(rows[filerow]);
        }

        return;
      }
    }
  }
}

void Editor::setStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(statusmsg, sizeof(statusmsg), fmt, ap);
  va_end(ap);
  statusmsg_time = time(NULL);
}

void Editor::updateSyntax(Row& row) {
  row.hl.resize(row.render.length());
  std::fill(row.hl.begin(), row.hl.end(), HL::NORMAL);

  if (syntax == std::nullopt) {
    return;
  }

  auto& scs = syntax->singleline_comment_start;
  auto& mcs = syntax->multiline_comment_start;
  auto& mce = syntax->multiline_comment_end;

  int scs_len = scs.length();
  int mcs_len = mcs.length();
  int mce_len = mce.length();

  bool prev_sep = true;
  int in_string = 0;
  int in_comment = (row.idx > 0 && rows[row.idx - 1].hl_open_comment);

  std::size_t i = 0;
  while (i < row.render.length()) {
    char c = row.render[i];
    HL prev_hl = (i > 0) ? row.hl[i - 1] : HL::NORMAL;

    if (scs_len && !in_string && !in_comment) {
      if (!row.render.compare(i, scs_len, scs)) {
        std::fill(row.hl.begin() + i, row.hl.end(), HL::COMMENT);
        break;
      }
    }

    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row.hl[i] = HL::MLCOMMENT;
        if (!row.render.compare(i, mce_len, mce)) {
          std::fill(row.hl.begin() + i, row.hl.begin() + i + mce_len,
            HL::COMMENT);
          i += mce_len;
          in_comment = 0;
          prev_sep = true;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!row.render.compare(i, mcs_len, mcs)) {
        std::fill(row.hl.begin() + i, row.hl.begin() + i + mce_len, HL::COMMENT);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    if (syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row.hl[i] = HL::STRING;
        if (c == '\\' && i + 1 < row.render.length()) {
          row.hl[i + 1] = HL::STRING;
          i += 2;
          continue;
        }
        if (c == in_string) {
          in_string = 0;
        }
        i++;
        prev_sep = true;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row.hl[i] = HL::STRING;
          i++;
          continue;
        }
      }
    }

    if (syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL::NUMBER)) ||
      (c == '.' && prev_hl == HL::NUMBER)) {
        row.hl[i] = HL::NUMBER;
        i++;
        prev_sep = false;
        continue;
      }
    }

    if (prev_sep) {
      bool found = false;
      for (auto& keyword: syntax->keywords) {
        auto klen = keyword.length();
        int kw2 = keyword[klen - 1] == '|';
        if (kw2) {
          klen--;
        }
        if (!row.render.compare(i, klen, keyword) &&
            is_separator(row.render[i + klen])) {
            std::fill(row.hl.begin() + i, row.hl.begin() + i + klen,
              kw2 ? HL::KEYWORD2 : HL::KEYWORD1);

          i += klen;
          found = true;
          break;
        }
      }
      if (!found) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row.hl_open_comment != in_comment);
  row.hl_open_comment = in_comment;
  if (changed && row.idx + 1 < rows.size()) {
    updateSyntax(rows[row.idx + 1]);
  }
}
