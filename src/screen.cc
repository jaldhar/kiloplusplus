#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "screen.h"

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

void Screen::inverse(bool on) {
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
    die("write");
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
