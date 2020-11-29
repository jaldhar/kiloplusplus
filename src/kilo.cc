/*** includes ***/

#define _POSIX_C_SOURCE 200809L

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "row.h"
#include "syntax.h"

/*** defines ***/

constexpr const char* KILO_VERSION = "0.0.1";

constexpr const int KILO_QUIT_TIMES = 3;

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum class FGColor {
  BLACK   = 30,
  RED     = 31,
  GREEN   = 32,
  YELLOW  = 33,
  BLUE    = 34,
  MAGENTA = 35,
  CYAN    = 36,
  WHITE   = 37,
  RESET   = 39
};

/*** data ***/

struct Screen {
  Screen();
  ~Screen();

  bool clear();
  void clearToEOL();
  void die(const char *s);
  void disableRawMode();
  void enableRawMode();
  bool getCursorPosition();
  bool getWindowSize();
  void hideCursor();
  void inverse(bool);
  void moveCursor(std::size_t, std::size_t);
  void print(const char*, std::size_t);
  int  readKey();
  void refresh();
  void setFGColor(FGColor);
  void showCursor();

  int cols;
  int rows;
  struct termios orig_termios;
  std::string ab;
} screen;

Screen::Screen() : cols{0}, rows{0}, orig_termios{}, ab{} {
  if (!getWindowSize()) {
    die("getWindowSize");
  }
  rows -= 2;
  enableRawMode();
}

Screen::~Screen() {
  disableRawMode();
}

bool Screen::clear() {
  if (write(STDOUT_FILENO, "\x1b[2J", 4) == -1) {
    return false;
  }

  if (write(STDOUT_FILENO, "\x1b[H", 3) == -1) {
    return false;
  }

  return true;
}

void Screen::clearToEOL() {
  ab.append("\x1b[K", 3);
}

void Screen::die(const char *s) {
  if (!clear()) {
    perror("write");
  }

  perror(s);
  exit(1);
}

void Screen::disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  }
}

void Screen::enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tcgetattr");
  }

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

bool Screen::getCursorPosition() {

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return false;
  }

  char buf[32];
  unsigned int i = 0;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R') {
      break;
    }
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') {
    return false;
  }
  if (sscanf(&buf[2], "%d;%d", &rows, &cols) != 2) {
    return false;
  }

  return true;
}

bool Screen::getWindowSize() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return false;
    }
    return getCursorPosition();
  } else {
    cols = ws.ws_col;
    rows = ws.ws_row;
    return true;
  }
}

void Screen::hideCursor() {
  ab.append("\x1b[?25l", 6);
}

void Screen::inverse(bool on = true) {
  if (on) {
    ab.append("\x1b[7m", 4);
  } else {
    ab.append("\x1b[m", 3);
  }
}

void Screen::moveCursor(std::size_t row, std::size_t col) {
    if (row == 0 && col == 0) {
        ab.append("\x1b[H", 3);
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "\x1b[%ld;%ldH", row, col);
      ab.append(buf, strlen(buf));
    }
}

void:: Screen::print(const char* s, std::size_t len) {
  ab.append(s, len);
}

int Screen::readKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
      return '\x1b';
    }

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) {
          return '\x1b';
        }
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

void Screen::refresh() {
  if (write(STDOUT_FILENO, ab.data(), ab.size()) == -1) {
    screen.die("write");
  }
  ab.clear();
}

void Screen::setFGColor(FGColor color) {
  char buf[16];
  int clen =
    snprintf(buf, sizeof(buf), "\x1b[%dm", static_cast<int>(color));
  ab.append(buf, clen);
}

void Screen::showCursor() {
  ab.append("\x1b[?25h", 6);
}

struct EditorConfig {
  EditorConfig();
  EditorConfig(const EditorConfig&)=delete;
  EditorConfig& operator=(const EditorConfig&)=delete;

  std::size_t cx, cy;
  std::size_t rx;
  std::size_t rowoff;
  std::size_t coloff;
  std::vector<Row> rows;
  bool dirty;
  std::string filename;
  char statusmsg[80];
  time_t statusmsg_time;
  std::optional<EditorSyntax> syntax;
} E;

EditorConfig::EditorConfig() : cx{0}, cy{0}, rx{0}, rowoff{0}, coloff{0},
rows{}, dirty{false}, filename{}, statusmsg{0}, statusmsg_time{0},
syntax{std::nullopt} {
}

/*** filetypes ***/

std::vector<EditorSyntax> hldb {
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
};

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
std::string editorPrompt(const char *prompt, void (*callback)(std::string&, int));

/*** syntax highlighting ***/

bool is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(Row& row) {
  row.hl.resize(row.render.length());
  std::fill(row.hl.begin(), row.hl.end(), HL::NORMAL);

  if (E.syntax == std::nullopt) {
    return;
  }

  auto& scs = E.syntax->singleline_comment_start;
  auto& mcs = E.syntax->multiline_comment_start;
  auto& mce = E.syntax->multiline_comment_end;

  int scs_len = scs.length();
  int mcs_len = mcs.length();
  int mce_len = mce.length();

  bool prev_sep = true;
  int in_string = 0;
  int in_comment = (row.idx > 0 && E.rows[row.idx - 1].hl_open_comment);

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

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
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

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
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
      for (auto& keyword: E.syntax->keywords) {
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
  if (changed && row.idx + 1 < E.rows.size()) {
    editorUpdateSyntax(E.rows[row.idx + 1]);
  }
}

FGColor editorSyntaxToColor(HL hl) {
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

void editorSelectSyntaxHighlight() {
  E.syntax = std::nullopt;

  if (E.filename.empty()) {
    return;
  }

  auto pos = E.filename.find_last_of('.');
  pos = (pos == std::string::npos) ? 0 : pos;
  std::string ext(E.filename, pos);

  for (auto& hl: hldb) {
    for (auto& match: hl.filematch) {
      auto is_ext = (match[0] == '.');
      if ((is_ext && ext != E.filename && ext == match) ||
      (!is_ext && E.filename.find(match) != std::string::npos)) {
        E.syntax = hl;

        for (std::size_t filerow = 0; filerow < E.rows.size(); filerow++) {
          editorUpdateSyntax(E.rows[filerow]);
        }

        return;
      }
    }
  }
}

/*** row operations ***/

void editorInsertRow(std::size_t at, const char *s) {
  if (at > E.rows.size()) {
    return;
  }

  E.rows.emplace(E.rows.begin() + at, at, s);
  for (auto j = at + 1; j < E.rows.size(); j++) {
    E.rows[j].idx++;
  }

  E.rows[at].update();

  E.dirty = true;
}

void editorDelRow(std::size_t at) {
  if (at >= E.rows.size()) {
    return;
  }
  E.rows.erase(E.rows.begin() + at);
  E.dirty = true;
}

/*** editor operations ***/

void editorInsertChar(int c) {
  if (E.cy == E.rows.size()) {
    editorInsertRow(E.rows.size(), "");
  }
  E.rows[E.cy].insert(E.cx, c);
  E.dirty = true;
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "");
  } else {
    Row *row = &E.rows[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx]);
    row = &E.rows[E.cy];
    row->chars.erase(E.cx);
    row->update();
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.rows.size()) {
    return;
  }
  if (E.cx == 0 && E.cy == 0) {
    return;
  }

  Row *row = &E.rows[E.cy];
  if (E.cx > 0) {
    row->erase(E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.rows[E.cy - 1].chars.length();
    E.rows[E.cy - 1].append(row->chars);
    editorDelRow(E.cy);
    E.cy--;
  }
  E.dirty = true;
}

/*** file i/o ***/

std::string editorRowsToString() {
  std::string buf;

  for (std::size_t j = 0; j < E.rows.size(); j++) {
    buf += E.rows[j].chars + '\n';
  }

  return buf;
}

void editorOpen(const char *filename) {
  E.filename = filename;

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
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
    editorInsertRow(E.rows.size(), line);
    E.rows[E.rows.size() - 1].erase(linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = false;
}

void editorSave() {
  if (E.filename.empty()) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename.empty()) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  std::string buf = editorRowsToString();

  int fd = open(E.filename.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, buf.length()) != -1) {
      if (write(fd, buf.data(), buf.length()) ==
      static_cast<ssize_t>(buf.length())) {
        close(fd);
        E.dirty = false;
        editorSetStatusMessage("%ld bytes written to disk", buf.length());
        return;
      }
    }
    close(fd);
  }

  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find ***/

void editorFindCallback(std::string& query, int key) {
  static std::optional<std::size_t> last_match = std::nullopt;
  static int direction = 1;

  static int saved_hl_line;
  static Highlight saved_hl;

  if (!saved_hl.empty()) {
    E.rows[saved_hl_line].hl = saved_hl;
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
  for (std::size_t i = 0; i < E.rows.size(); i++) {
    *current += direction;
    if (current == std::nullopt) {
      current = E.rows.size() - 1;
    } else if (*current == E.rows.size()) {
      current = 0;
    }

    Row& row = E.rows[*current];
    auto match = row.render.find(query);
    if (match != std::string::npos) {
      last_match = *current;
      E.cy = *current;
      E.cx = row.rxtocx(match);
      E.rowoff = E.rows.size();

      saved_hl_line = *current;
      saved_hl = row.hl;
      std::fill(row.hl.begin() + match,
        row.hl.begin() + match + query.length(), HL::MATCH);
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  std::string query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)",
    editorFindCallback);

  if (query.empty()) {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** input ***/
std::string editorPrompt(const char *prompt, void (*callback)(std::string&, int)) {
  std::string buf;

  while (true) {
    editorSetStatusMessage(prompt, buf.c_str());
    editorRefreshScreen();

    int c = screen.readKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
    }
    else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) {
        callback(buf, c);
      }
      return "";
    } else if (c == '\r') {
      if (!buf.empty()) {
        editorSetStatusMessage("");
        if (callback) {
          callback(buf, c);
        }
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      buf += c;
    }

    if (callback) {
      callback(buf, c);
    }
  }
}

void editorMoveCursor(int key) {
  std::optional<std::reference_wrapper<Row>> row = (E.cy >= E.rows.size())
    ? std::nullopt
    : std::make_optional(std::ref(E.rows[E.cy]));

  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.rows[E.cy].chars.length();
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->get().chars.length()) {
        E.cx++;
      } else if (row && E.cx == row->get().chars.length()) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.rows.size()) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.rows.size())
    ? std::nullopt
    : std::make_optional(std::ref(E.rows[E.cy]));
  std::size_t rowlen = row ? row->get().chars.length() : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {
  static int quit_times = KILO_QUIT_TIMES;

  int c = screen.readKey();

  switch (c) {
    case '\r':
      editorInsertNewline();
      break;

    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. "
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
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      if (E.cy < E.rows.size()) {
        E.cx = E.rows[E.cy].chars.length();
      }
      break;

    case CTRL_KEY('f'):
      editorFind();
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) {
        editorMoveCursor(ARROW_RIGHT);
      }
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + screen.rows - 1;
          if (E.cy > E.rows.size()) {
            E.cy = E.rows.size();
          }
        }

        int times = screen.rows;
        while (times--) {
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }

  quit_times = KILO_QUIT_TIMES;
}

/*** output ***/

void editorScroll() {
  E.rx = 0;

  if (E.cy < E.rows.size()) {
    E.rx = E.rows[E.cy].cxtorx(E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + screen.rows) {
    E.rowoff = E.cy - screen.rows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + screen.cols) {
    E.coloff = E.rx - screen.cols + 1;
  }
}

void editorDrawRows() {
  for (auto y = 0; y < screen.rows; y++) {
    std::size_t filerow = y + E.rowoff;
    if (filerow >= E.rows.size()) {
      if (E.rows.size() == 0 && y == screen.rows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > screen.cols) {
          welcomelen = screen.cols;
        }
        int padding = (screen.cols - welcomelen) / 2;
        if (padding) {
          screen.print("~", 1);
          padding--;
        }
        while (padding--) {
          screen.print(" ", 1);
        }
        screen.print(welcome, welcomelen);
      } else {
        screen.print("~", 1);
      }
    } else {
      int len = E.rows[filerow].render.length() - E.coloff;
      if (len < 0) {
        len = 0;
      }
      if (len > screen.cols) {
        len = screen.cols;
      }
      char *c = &E.rows[filerow].render[E.coloff];
      HL* hl = &E.rows[filerow].hl[E.coloff];
      FGColor current_color = FGColor::RESET;
      for (auto j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          screen.inverse();
          screen.print(&sym, 1);
          screen.inverse(false);
          if (current_color != FGColor::RESET) {
            screen.setFGColor(current_color);
          }
        } else if (hl[j] == HL::NORMAL) {
          if (current_color != FGColor::RESET) {
            screen.setFGColor(FGColor::RESET);
            current_color = FGColor::RESET;
          }
          screen.print(&c[j], 1);
        } else {
          FGColor color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            screen.setFGColor(color);
          }
          screen.print(&c[j], 1);
        }
      }
      screen.setFGColor(FGColor::RESET);
    }

    screen.clearToEOL();
    screen.print("\r\n", 2);
  }
}

void editorDrawStatusBar() {
  screen.inverse();
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %ld lines %s",
    E.filename.empty() ? "[No Name]" : E.filename.c_str(), E.rows.size(),
    E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %ld/%ld",
    E.syntax ? E.syntax->filetype.c_str() : "no ft", E.cy + 1, E.rows.size());
  if (len > screen.cols) {
    len = screen.cols;
  }
  screen.print(status, len);
  while (len < screen.cols) {
    if (screen.cols - len == rlen) {
      screen.print(rstatus, rlen);
      break;
    } else {
      screen.print(" ", 1);
      len++;
    }
  }
  screen.inverse(false);
  screen.print("\r\n", 2);
}

void editorDrawMessageBar() {
  screen.clearToEOL();
  int msglen = strlen(E.statusmsg);
  if (msglen > screen.cols) {
    msglen = screen.cols;
  }
  if (msglen && time(NULL) - E.statusmsg_time < 5) {
    screen.print(E.statusmsg, msglen);
  }
}

void editorRefreshScreen() {
  editorScroll();

  screen.hideCursor();
  screen.moveCursor(0, 0);

  editorDrawRows();
  editorDrawStatusBar();
  editorDrawMessageBar();

  screen.moveCursor((E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
  screen.showCursor();

  screen.refresh();
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** init ***/

int main(int argc, const char *argv[]) {
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  while (true) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}