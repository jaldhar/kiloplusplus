#include "editor.h"
#include "screen.h"

int main(int argc, const char *argv[]) {
  Editor editor;
  Screen screen;

  try {
    if (argc >= 2) {
      editor.openFile(screen, argv[1]);
    }

    editor.setStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    bool running = true;
    while (running) {
      editor.draw(screen);
      running = editor.processKeypress(screen);
    }

  } catch(std::string& e) {
    fprintf(stderr, "%s\n", e.c_str());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
