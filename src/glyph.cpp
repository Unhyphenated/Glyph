/*** Includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <vector>
#include <time.h>
#include "Abuf.h"
#include <iostream>
#include <string>
#include <stdarg.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define GLYPH_VERSION "0.0.1"
#define GLYPH_TAB_STOP 8
#define GLYPH_QUIT_COUNT 3

enum cursorKeys {
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
};

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD_1,
    HL_KEYWORD_2,
    HL_STRING,
    HL_DIGIT,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

struct editorSyntax {
    const char *filetype;
    const char **filematch;
    const char **keywords;
    const char *singleline_comment_start;
    const char *multiline_comment_start;
    const char *multiline_comment_end;
    int flags;
};

typedef struct erow {
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
} erow;

/*** Data ***/
struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff, coloff;
    int screenrows;
    int screencols;
    int numrows;
    int dirty;
    erow *row;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios original_termios;
};

/*** Filetypes ***/
const char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL};
const char *C_HL_keywords[] = { "switch", "if", "while", "for", "break", 
"continue", "return", "else", "struct", "union", "typedef", "static",
"enum", "class", "case", "int|", "long|", "double|", "float|", "char|",
"unsigned|", "signed|", "void|", NULL};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

struct editorConfig E;
void editorSetStatusMessage(const char *fmt, ...);
void editorDelChar();
void editorInsertNewLine();
int editorReadKey();
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

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
void editorInsertChar(int c);
void editorSave();
void editorFind();

char *editorPrompt(const char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = (char *)malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        
        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == END_KEY) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = (char *)realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if (callback) callback(buf, c);
    }
}

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
    static int quit_count = GLYPH_QUIT_COUNT;
    int c = editorReadKey();

    switch(c) {
        case '\r':
            editorInsertNewLine();
            break;
        // Quit the program when 'Ctrl-Q' is used
        case CTRL_KEY('q'):
            if (E.dirty && quit_count > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", quit_count);
                quit_count--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case BACKSPACE:
        case DEL_KEY:
        case CTRL_KEY('h'):
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;
        
        case HOME_KEY: 
            E.cx = 0;
            break;

        case END_KEY:
            if (E.cy < E.numrows) {
                E.cx = E.row[E.cy].size;
            }
            break;

        case CTRL_KEY('f'):
            editorFind();
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

        case CTRL_KEY('s'):
            editorSave();
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        // Move cursor using "WASD"
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        default:
            editorInsertChar(c);
            break;
    }
    quit_count = GLYPH_QUIT_COUNT;
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

int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row -> size; cx++) {
        if (row -> chars[cx] == '\t')
            cx += (GLYPH_TAB_STOP - 1) - (cur_rx % GLYPH_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx) return cx;
    }
    return cx;
}

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
    row -> hl = (unsigned char *)realloc(row -> hl, row -> rsize);
    memset(row -> hl, HL_NORMAL, row -> rsize);

    if (E.syntax == NULL) return;

    const char **keywords = E.syntax -> keywords;

    const char *scs = E.syntax -> singleline_comment_start;
    const char *mcs = E.syntax -> multiline_comment_start;
    const char *mce = E.syntax -> multiline_comment_end;

    int scs_length = scs ? strlen(scs) : 0;
    int mcs_length = mcs ? strlen(mcs) : 0;
    int mce_length = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row -> idx > 0 && E.row[row -> idx - 1].hl_open_comment);

    int i = 0;
    while (i < row -> rsize) {
        char c = row -> render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_length && !in_string && !in_comment) {
            if (!strncmp(&row -> render[i], scs, scs_length)) {
                memset(&row -> hl[i], HL_COMMENT, row -> rsize - i);
                break;
            }
        }
        if (mcs_length && mce_length && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row -> render[i], mce, mce_length)) {
                    memset(&row -> hl[i], HL_MLCOMMENT, mce_length);
                    i += mce_length;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row -> render[i], mcs, mcs_length)) {
                memset(&row -> hl[i], HL_MLCOMMENT, mcs_length);
                i += mcs_length;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax -> flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row -> hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row -> rsize) {
                    row -> hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row -> hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        
        if (E.syntax -> flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_DIGIT)) ||
                (c == '.' && prev_hl == HL_DIGIT)) {
                row -> hl[i] = HL_DIGIT;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2) klen--;

                if (!strncmp(&row -> render[i], keywords[j], klen) && 
                is_separator(row -> render[klen + i])) {
                    memset(&row -> hl[i], kw2 ? HL_KEYWORD_2 : HL_KEYWORD_1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row -> hl_open_comment != in_comment);
    row -> hl_open_comment = in_comment;
    if (changed && row -> idx + 1 < E.numrows) editorUpdateSyntax(&E.row[row -> idx + 1]);
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case (HL_MLCOMMENT):
        case (HL_COMMENT): return 36;
        case (HL_STRING): return 35;
        case (HL_KEYWORD_1): return 33;
        case (HL_KEYWORD_2): return 32;
        case (HL_DIGIT): return 31;
        case (HL_MATCH): return 34;
        default: return 37;
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL) return;

    char *ext = strchr(E.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s -> filematch[i]) {
            int is_ext = (s -> filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s -> filematch[i])) || 
            (!is_ext && strstr(E.filename, s -> filematch[i]))) {
                E.syntax = s;
                
                int filerow;
                for (filerow = 0; filerow < E.numrows; filerow++) {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}
/*** Output ***/
void editorDrawStatusBar(Abuf& ab) {
    ab.append("\x1b[7m", 4);
    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", 
    E.filename ? E.filename : "[Untitled]", E.numrows,
    E.dirty ? "(modified)" : "");

    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d%d", E.syntax ? E.syntax -> filetype : "None",
    E.cy + 1, E.numrows);
    
    if (len > E.screencols) len = E.screencols;
    ab.append(status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            ab.append(rstatus, rlen);
            break;
        } else {
            len++;
            ab.append(" ", 1);
        }
    }
    ab.append("\x1b[m", 3);
    ab.append("\r\n", 2);
}

void editorDrawStatusMessage(Abuf& ab) {
    ab.append("\x1b[K", 3);
    int len = strlen(E.statusmsg);
    if (len > E.screencols) len = E.screencols;
    if (len && time(NULL) - E.statusmsg_time < 5) ab.append(E.statusmsg, len);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

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
    editorDrawStatusBar(AB);
    editorDrawStatusMessage(AB);
    
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
            if (E.numrows == 0 && y == E.screenrows / 2) {
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
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    ab.append("\x1b[7m", 4);
                    ab.append(&sym, 1);
                    ab.append("\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        ab.append(buf, clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        ab.append("\x1b[39m", 5);
                        current_color = -1;
                    }
                    ab.append(&c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab.append(buf, clen);
                    }
                    ab.append(&c[j], 1);
                }
                
            }
            ab.append("\x1b[39m", 5);
        }

        ab.append("\x1b[K", 3);
        ab.append("\r\n", 2);
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
    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;

    E.row = (erow* )realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows + 1));
    for (int j = at + 1; j < E.numrows; j++) E.row[j].idx++;

    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = (char* )malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);
    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row) {
    free(row -> render);
    free(row -> chars);
    free(row -> hl);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    for (int j = at; j < E.numrows; j++) E.row[at].idx--;
    E.numrows--;
    E.dirty++;
}

/*** Editor Operations ***/
void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row -> size) at = row -> size;
    row -> chars = (char *)realloc(row -> chars, row -> size + 2);
    memmove(&row -> chars[at + 1], &row -> chars[at], row -> size - at + 1);
    row -> size++;
    row -> chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
    row -> chars = (char *)realloc(row -> chars, row -> size + len + 1);
    memcpy(&row -> chars[row -> size], s, len);
    row -> size += len;
    row -> chars [row -> size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at > row -> size)  at = row -> size;
    memmove(&row -> chars[at], &row -> chars[at + 1], row -> size - at);
    row -> size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorInsertChar(int c) {
    char *emptyRow;
    if (E.cy == E.numrows) {
        editorInsertRow(E.numrows, emptyRow, 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewLine() {
    char *emptyRow;
    if (E.cx == 0) {
        editorInsertRow(E.cy, emptyRow, 0);
    } else {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row -> chars[E.cx], row -> size - E.cx);
        row = &E.row[E.cy];
        row -> size = E.cx;
        row -> chars[E.cx] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar() {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row -> chars, row -> size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

char *editorRowsToString(int *buflen) {
    int totlen = 0;
    for (int j = 0; j < E.numrows; j++) {
        totlen += E.row[j].size + 1;
    }
    *buflen = totlen;
    char *buf = (char *)malloc(totlen);
    char *p = buf;

    for (int i = 0; i < E.numrows; i++) {
        memcpy(p, E.row[i].chars, E.row[i].size);
        p += E.row[i].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Saved failed! I/O error: %s", strerror(errno));
}

void openEditor(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();
    FILE *fp = fopen(filename, "r");
    if (!fp) die ("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && line[linelen - 1] == '\r' || line[linelen - 1] == '\n') linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

/*** Search ***/
void editorFindCallBack(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_DOWN || key == ARROW_RIGHT) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;

    int i;
    for (i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1) current = E.numrows - 1;
        else if (current == E.numrows) current = 0;
    
        erow *row = &E.row[current];
        char *match = strstr(row -> render, query);
        if (match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row -> render);
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = (char *)malloc(row -> rsize);
            memcpy(saved_hl, row -> hl, row -> rsize);

            memset(&row -> hl[match - row -> render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallBack);
    if (query) {
        free(query);
    } else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
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
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.dirty = 0;
    E.syntax = NULL;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        openEditor(argv[1]);
    }
    editorSetStatusMessage("HELP: Ctrl-S = Save | Ctrl-Q = Quit | Ctrl-F = Find");

    // Quit terminal when 'q' is typed.
    while (1) {
        editorRefreshScreen();
        editorProcessKey();
    }

    return 0;
}