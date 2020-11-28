#ifndef ROW_H
#define ROW_H

#include <string>

enum class HL : unsigned char;

using Highlight = std::basic_string<HL>;

struct Row {
  explicit Row(std::size_t, const char*);

  void append(std::string);
  void erase(std::size_t);
  void insert(std::size_t, int);
  void update();

  int  cxtorx(int);
  std::size_t rxtocx(int);

  std::size_t idx;
  std::string chars;
  std::string render;
  Highlight   hl;
  int         hl_open_comment;
};

#endif