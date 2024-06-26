/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <vector>

#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/
struct editorConfig {
    int screenrows;
    int screencols;
  struct termios original_termios;
};

struct editorConfig E;

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
    write(STDOUT_FILENO, "\x1b[H", 3); // Repositions the cursor
    // Display error message based on global errno variable
    perror(s);
    exit(1);
}

/*** Terminal ***/
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
    // Get the terminal attributes of a standard terminal
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    // Create a reference to the original termios
    struct termios raw = E.original_termios;

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

/*** Input ***/
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
        write(STDOUT_FILENO, "\x1b[H", 3); // Repositions the cursor
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

/*** Output ***/
void editorDrawRows(); // Initialise function that will be defined later (this causes an error is omitted)
void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
    write(STDOUT_FILENO, "\x1b[H", 3); // Repositions the cursor
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3); // Repositions the cursor
}

void editorDrawRows(class Abuf ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {

        if (y == E.screenrows - 1) {
            ab.append("~", 1);
            break;
        }
        ab.append("~\r\n", 3);
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Append Buffer ***/
class Abuf {
    private:
        std::vector<char> buffer;

    public:
        Abuf() = default;

        void append(const char* s, int len) {
            if (s == nullptr || len <= 0) return;
            buffer.insert(buffer.end(), s, s + len);
        }

        // Destructor - vector handles its own memory cleanup
        ~Abuf() = default;
};


/*** Init ***/
void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    // Quit terminal when 'q' is typed. Otherwise, display the the character
    // and it's ASCII value
    // while (1) {
    editorRefreshScreen();
    editorProcessKey();
    // }

    return 0;
}