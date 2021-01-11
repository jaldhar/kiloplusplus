#ifndef EDITOR_H
#define EDITOR_H

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "row.h"

enum class HL : unsigned char {
  NORMAL = 0,
  COMMENT,
  MLCOMMENT,
  KEYWORD1,
  KEYWORD2,
  STRING,
  NUMBER,
  MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

struct EditorSyntax {
  std::string filetype;
  std::vector<const char*> filematch;
  std::vector<std::string> keywords;
  std::string singleline_comment_start;
  std::string multiline_comment_start;
  std::string multiline_comment_end;
  int flags;
};

struct Screen;

struct Editor {
  Editor();
  ~Editor() {
  }

  void delChar();
  void delRow(std::size_t);
  void draw(Screen&);
  void drawMessageBar(Screen&);
  void drawRows(Screen&);
  void drawStatusBar(Screen&);
  void find(Screen&);
  void findCallback(std::string&, int);
  void insertChar(int);
  void insertNewline();
  void insertRow(std::size_t, const char*);
  void moveCursor(int);
  void openFile(Screen&, const char*);
  void processKeypress(Screen&);
  std::string prompt(Screen&, const char*,
  std::optional<std::function<void(Editor*, std::string&, int)>>);
  void saveFile(Screen&);
  void scroll(Screen&);
  void selectSyntaxHighlight();
  void setStatusMessage(const char *fmt, ...);
  void updateSyntax(Row& row);

  Editor(const Editor&)=delete;
  Editor& operator=(const Editor&)=delete;

  std::size_t cx, cy;
  std::size_t rx;
  std::size_t rowoff;
  std::size_t coloff;
  std::vector<Row> rows;
  bool dirty;
  std::filesystem::path filename;
  char statusmsg[80];
  time_t statusmsg_time;
  std::optional<EditorSyntax> syntax;
  std::vector<EditorSyntax> hldb;
};

#endif