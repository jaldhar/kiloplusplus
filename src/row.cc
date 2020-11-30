#include "row.h"

constexpr const std::size_t KILO_TAB_STOP = 8;

Row::Row(std::size_t at, const char *s) : idx{at}, chars{s},
render{}, hl{}, hl_open_comment{0} {
}

void Row::append(std::string s) {
  chars += s;
  update();
}

void Row::erase(std::size_t at) {
  if (at >= chars.length()) {
    return;
  }
  chars.erase(at, 1);
  update();
}

void Row::insert(std::size_t at, int c) {
  if (at > chars.length()) {
    at = chars.length();
  }
  chars.insert(at, 1, c);
  update();
}

void Row::update() {
  int tabs = 0;
  for (auto& j: chars) {
    if (j == '\t') {
      tabs++;
    }
  }

  render.resize(chars.length() + tabs*(KILO_TAB_STOP - 1));

  int idx = 0;
  for (auto& j: chars) {
    if (j == '\t') {
      render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) {
        render[idx++] = ' ';
      }
    } else {
      render[idx++] = j;
    }
  }
}

int Row::cxtorx(int cx) {
  int rx = 0;
  for (auto j = 0; j < cx; j++) {
    if (chars[j] == '\t') {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

std::size_t Row::rxtocx(int rx) {
  int cur_rx = 0;
  std::size_t cx;
  for (cx = 0; cx < chars.length(); cx++) {
    if (chars[cx] == '\t') {
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
    }
    cur_rx++;

    if (cur_rx > rx) {
      return cx;
    }
  }
  return cx;
}
