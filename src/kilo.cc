#include "editor.h"
#include "screen.h"

Editor E;
Screen screen;

int main(int argc, const char *argv[]) {

  try {
    if (argc >= 2) {
      E.openFile(screen, argv[1]);
    }

    E.setStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (true) {
      E.draw(screen);
      E.processKeypress(screen);
    }

  } catch(std::string& e) {
    fprintf(stderr, "%s\n", e.c_str());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
