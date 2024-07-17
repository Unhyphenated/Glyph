/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <vector>
#include "Abuf.h"
#include <iostream>

#define CTRL_KEY(k) ((k) & 0x1f)
#define GLYPH_VERSION "0.0.1"
#define GLYPH_TAB_STOP 8

enum cursorKeys {
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

typedef struct erow {
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;

/*** Data ***/
struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff, coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
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
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '2': return END_KEY;
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            switch(seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY; 
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key) {
        case ARROW_UP:
        if (E.cy != 0) E.cy--;
            break;

        case ARROW_LEFT:
        if (E.cx != 0) {
            E.cx--;
        } else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
            break;

        case ARROW_DOWN:
        if (E.cy < E.numrows) E.cy++;
            break;

        case ARROW_RIGHT:
        if (row && E.cx < row -> size) {
            E.cx++;
        } else if (row && E.cx == row -> size) {
            E.cy++;
            E.cx = 0;
        }
            break;
    }
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row -> size : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

void editorProcessKey() {
    int c = editorReadKey();

    switch(c) {
        // Quit the program when 'Ctrl-Q' is used
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case HOME_KEY: 
            E.cx = 0;
            break;

        case END_KEY:
            if (E.cy < E.numrows) {
                E.cx = E.row[E.cy].size;
            }
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP) {
                    E.cy = E.rowoff;
                } else if (c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > E.numrows) E.cy = E.numrows;
                }
                int times = E.screenrows;
                while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        // Move cursor using "WASD"
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row -> chars[j] == '\t')
            rx += (GLYPH_TAB_STOP - 1) - (rx % GLYPH_TAB_STOP);
        rx++;
    }
    return rx;
}

/*** Output ***/
void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }
    if (E.cy < E.rowoff) E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
    if (E.cx < E.coloff) E.coloff = E.rx;
    if (E.cx >= E.coloff + E.screencols) E.coloff = E.rx - E.screencols + 1;
}
void editorDrawRows(Abuf& ab); // Initialise function that will be defined later (this causes an error is omitted)
void editorRefreshScreen() {
    editorScroll();
    Abuf AB = Abuf();
    AB.append("\x1b[?25l", 6); // Erase cursor
    AB.append("\x1b[H", 3); // Repositions the cursor
    editorDrawRows(AB);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rowoff + 1, E.rx - E.coloff + 1);
    AB.append(buf, strlen(buf));

    AB.append("\x1b[?25h", 6); // Draws cursor
    write(STDOUT_FILENO, AB.data(), AB.size());
}

void editorDrawRows(Abuf& ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Glyph Editor -- version %s", GLYPH_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    ab.append("~", 1);
                    padding--;
                }
                while (padding--) ab.append(" ", 1);
                ab.append(welcome, welcomelen);
            } else {
                ab.append("~", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            ab.append(&E.row[filerow].render[E.coloff], len);
        }

        ab.append("\x1b[K", 3);
        if (y < E.screenrows - 1) ab.append("\r\n", 2);
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

void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row -> size; j++)
    if (row -> chars[j] == '\t') tabs++;

    free(row -> render);
    row->render = (char *)malloc(row -> size + (tabs * GLYPH_TAB_STOP - 1) + 1);
    
    int idx = 0;
    for (j = 0; j < row -> size; j++) {
        if (row -> chars[j] == '\t') {
            row -> render[idx++] = ' ';
            while (idx % GLYPH_TAB_STOP != 0) row -> render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
    E.row = (erow* )realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;

    E.row[at].size = len;
    E.row[at].chars = (char* )malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    E.numrows++;
}

void openEditor(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) die ("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && line[linelen - 1] == '\r' || line[linelen - 1] == '\n') linelen--;
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** Init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numrows = 0;
    E.row = NULL;
    E.rowoff = 0;
    E.coloff = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        openEditor(argv[1]);
    }

    // Quit terminal when 'q' is typed.
    while (1) {
        editorRefreshScreen();
        editorProcessKey();
    }

    return 0;
}