/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/
struct termios original_termios;

void die(const char *s) {
    // Display error message based on global errno variable
    perror(s);
    exit(1);
}

/*** Terminal ***/
void disableRawMode() {
    if (tcsetattr(STDERR_FILENO, TCSAFLUSH, &original_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
    // Get the terminal attributes of a standard terminal
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    // Create a reference to the original termios
    struct termios raw = original_termios;

    // Disable 'Ctrl-S' and 'Ctrl-Q'
    // ICRNL stops terminal from translating new lines / carriage returns
    raw.c_iflag &= ~(BRKINT | ICRNL | IXON | INPCK | ISTRIP);

    // Disable carriage returns and newlines when 'Enter' is pressed
    raw.c_oflag &= ~(OPOST);
    // Set the 'ECHO' bitflag to 0 
        // ICANON is used to disable canonical mode allowing byte-by-byte
        // reading instead of the standard line-by-line
        // ISIG is used to disable 'Ctrl-Z' and 'Ctrl-C' suspensions
        // IEXTEN disables 'Ctrl-V' and 'Ctrl-O'
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= (CS8);

    // Set a timeout duration for reading input
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != -1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

void editorProcessKey() {
    char c = editorReadKey();

    // Quit the program when 'Ctrl-Q' is used
    switch(c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}
/*** Init ***/
int main() {
    enableRawMode();

    // Quit terminal when 'q' is typed. Otherwise, display the the character
    // and it's ASCII value
    while (1) {
       
    }

    return 0;
}