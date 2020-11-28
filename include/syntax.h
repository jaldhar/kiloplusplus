#ifndef SYNTAX_H
#define SYNTAX_H

#include <string>
#include <vector>

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

struct Row;

void editorUpdateSyntax(Row&);


#endif