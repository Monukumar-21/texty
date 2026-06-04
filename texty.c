#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <termios.h>

/*** defines ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define TEXTY_VERSION "0.0.1"
#define TEXTY_TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)   // Masks the character with 00011111 to simulate Ctrl+key

// Special keycodes returned by editorReadKey()
enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
};

/*** data ***/

// A single row of text: both the raw characters and the rendered version (tabs→spaces)
typedef struct erow {
  int size;           // length of the raw line
  int rsize;          // length of the rendered line
  char *chars;        // raw characters
  char *render;       // rendered characters (tabs expanded)
} erow;

// Global editor state
struct editorConfig {
  int cx, cy;         // cursor position (column, row) in the file
  int rx;             // rendered column index (if we have tabs)
  int rowoff;         // vertical scroll offset
  int coloff;         // horizontal scroll offset
  int screenrows;     // number of rows the terminal can display
  int screencols;     // number of columns
  int numrows;        // total lines in the file
  erow *row;          // dynamic array of lines
  char *filename;     // current file name
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;  // saved terminal attributes
};

struct editorConfig E;

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorRowsToString(int *buflen);

/*** terminal ***/

// Print an error and exit, restoring the screen first
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);   // clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);    // reposition cursor to home
  perror(s);
  exit(1);
}

// Restore original terminal attributes
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

// Put terminal into raw mode: no echo, no canonical processing, etc.
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);   // ensure we restore terminal on exit

  struct termios raw = E.orig_termios;
  // Input flags: turn off break, carriage-return-to-newline, parity check, strip 8th bit, software flow control
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // Output flags: disable post-processing (e.g., \n → \r\n)
  raw.c_oflag &= ~(OPOST);
  // Character size: 8 bits per byte
  raw.c_cflag |= (CS8);
  // Local flags: turn off echo, canonical mode, extended functions, signal chars
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // Timeout for read(): return after 1/10 second if no input
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Wait for a keypress and return the corresponding character or special key code
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  // If it's an escape sequence, parse it
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';  // unrecognized escape → treat as Escape key
  } else {
    return c;       // normal character
  }
}

// Ask terminal for cursor position (used as fallback for window size)
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

// Get terminal window size
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;   // fixed: only one declaration
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // Fallback: move cursor to bottom‑right corner and ask position
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

// Convert a cursor column index (in raw chars) to a rendered column index (after tab expansion)
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TEXTY_TAB_STOP - 1) - (rx % TEXTY_TAB_STOP);
    rx++;
  }
  return rx;
}

// Recalculate the rendered version of a row (expand tabs to spaces)
void editorUpdateRow(erow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs*(TEXTY_TAB_STOP - 1) + 1);
  int idx = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TEXTY_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

// Add a new row at the bottom of the file
void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
}

// Free the memory used by a row
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}

// Delete a row at index `at`
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.row = realloc(E.row, sizeof(erow) * E.numrows);
}

// Insert a character into a row at a given position
void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

// Append a string to the end of a row
void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
}

// Delete a character in a row at position `at`
void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
}

/*** editor operations ***/

// Insert a single character at the current cursor position
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    // Cursor is on the virtual line after the last line → add a new empty line first
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

// Insert a newline at cursor (split current line)
void editorInsertNewline() {
  if (E.cx == 0) {
    // At beginning of line, just insert an empty line above
    editorAppendRow("", 0);
    // Move rows down to make room for the new line at current position
    for (int i = E.numrows - 1; i > E.cy; i--) {
      E.row[i] = E.row[i - 1];
    }
    E.row[E.cy] = (erow){0, 0, NULL, NULL};
    editorAppendRow("", 0); // fix: simpler to use insert
    // Actually easier: use editorInsertRow
  } else {
    // Split current row into two
    erow *row = &E.row[E.cy];
    // Create new row with the part after cursor
    editorAppendRow(&row->chars[E.cx], row->size - E.cx);
    // Trim current row
    row->chars = realloc(row->chars, E.cx + 1);
    row->chars[E.cx] = '\0';
    row->size = E.cx;
    editorUpdateRow(row);
    // Move the newly added row to just below the current one
    erow newrow = E.row[E.numrows - 1];
    memmove(&E.row[E.cy + 2], &E.row[E.cy + 1], sizeof(erow) * (E.numrows - E.cy - 2));
    E.row[E.cy + 1] = newrow;
  }
  E.cy++;
  E.cx = 0;
}

// Delete the character before the cursor (Backspace)
void editorDelChar() {
  if (E.cy == E.numrows) return;   // nothing to delete on the virtual line
  if (E.cx == 0 && E.cy == 0) return; // beginning of file

  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    // At beginning of line: merge with previous line
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/*** file i/o ***/

// Convert the whole editor buffer to a single string (for saving)
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  for (int j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;   // +1 for newline
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (int j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

// Open a file and read its contents into the editor
void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    // Remove trailing newline / carriage return
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

// Save the current buffer to disk
void editorSave() {
  if (E.filename == NULL) {
    editorSetStatusMessage("No filename. Use Ctrl-S to save as? Not implemented.");
    return;
  }
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** append buffer (for efficient screen drawing) ***/

struct abuf {
  char *b;
  int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

// Update scroll offsets to keep the cursor visible
void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

// Draw the welcome message or the file rows
void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        // Welcome screen
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Texty editor -- version %s", TEXTY_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }
    // Clear the rest of the line
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

// Draw the status bar (bottom line of the screen)
void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);   // invert colors
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.filename ? "" : "(modified?)");   // you could track modified flag
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    E.cy + 1, E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);   // turn off inverted colors
  abAppend(ab, "\r\n", 2);
}

// Draw the message bar (below status bar)
void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);   // clear the line
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

// Refresh the screen
void editorRefreshScreen() {
  editorScroll();
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);   // hide cursor
  abAppend(&ab, "\x1b[H", 3);      // move to home position

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
           (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);   // show cursor again
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

// Set a temporary status message (disappears after 5 seconds)
void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/

// Move cursor in response to arrow keys
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx > 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy > 0) E.cy--;
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) E.cy++;
      break;
  }
  // Snap cursor to end of line if it's beyond the length
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) E.cx = rowlen;
}

// Process a single keypress
void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      // Quit without saving (you may want to add a confirmation)
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
      editorDelChar();
      break;

    case DEL_KEY:
      // Move cursor right then backspace (or implement directly)
      if (E.cy < E.numrows) {
        editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
      }
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    case '\r':   // Enter key
      editorInsertNewline();
      break;

    default:
      editorInsertChar(c);
      break;
  }
}

/*** init ***/

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;   // reserve two lines for status bar and message bar
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}