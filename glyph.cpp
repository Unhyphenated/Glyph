#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_termios;
void disableRawMode() {
    tcsetattr(STDERR_FILENO, TCSAFLUSH, &original_termios);
}

void enableRawMode() {
    // Get the terminal attributes of a standard terminal
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(disableRawMode);

    // Create a reference to the original termios
    struct termios raw = original_termios;

    // Set the 'ECHO' bitflag to 0
    raw.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    while (read(STDERR_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}