#include <ctype.h>
#include <stdio.h>
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
        // ICANON is used to disable canonical mode allowing byte-by-byte
        // reading instead of the standard line-by-line
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    // Quit terminal when 'q' is typed. Otherwise, display the the character
    // and it's ASCII value
    while (read(STDERR_FILENO, &c, 1) == 1 && c != 'q') {
        if (!iscntrl(c)) {
            printf("%d : %c\n", c, c);
        } else {
            printf("%d\n", c);
        }
    };
    return 0;
}