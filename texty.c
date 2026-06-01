#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <termios.h>//used for the terminal
struct termios orig_termios;

void disableRawMode(){
  tcsetattr(STDIN_FILENO,TCIFLUSH,&orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;//for automatic disabling the echo section
  //used to read terminal attributes
  // raw.c_lflag &= ~(ECHO | ICANON);//for reading input byte by byte rather than line by line
  raw.c_iflag &= ~(ICRNL | IXON);
  // raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);//tcsaflush is used
}

int main() {

  enableRawMode();//raw mode enable karne ka just like password in terminal
  char c;
  // while (read(STDIN_FILENO, &c, 1) == 1 && c!='q');
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
  }
  return 0;
}