#ifndef SCREEN_H
#define SCREEN_H

#include <string>
#include <termios.h>

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
  void inverse(bool = true);
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
};

#endif