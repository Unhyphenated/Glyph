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

    // Disable 'Ctrl-S' and 'Ctrl-Q'
    // ICRNL stops terminal from translating new lines / carriage returns
    raw.c_iflag &= ~(ICRNL | IXON);

    // Disable carriage returns and newlines when 'Enter' is pressed
    raw.c_oflag &= ~(OPOST);
    // Set the 'ECHO' bitflag to 0 
        // ICANON is used to disable canonical mode allowing byte-by-byte
        // reading instead of the standard line-by-line
        // ISIG is used to disable 'Ctrl-Z' and 'Ctrl-C' suspensions
        // IEXTEN disables 'Ctrl-V' and 'Ctrl-O'
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    // Quit terminal when 'q' is typed. Otherwise, display the the character
    // and it's ASCII value
    while (read(STDERR_FILENO, &c, 1) == 1 && c != 'q') {
        if (!iscntrl(c)) {
            printf("%d\r\n : %c\n", c, c);
        } else {
            printf("%d : '%c'\r\n", c, c);
        }
    };
    return 0;
}