#include "editor.h"
#include "screen.h"

Editor E;
Screen screen;

int main(int argc, const char *argv[]) {

  if (argc >= 2) {
    E.openFile(screen, argv[1]);
  }

  E.setStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  while (true) {
    E.draw(screen);
    E.processKeypress(screen);
  }

  return 0;
}
