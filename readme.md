The c_lflag field is for “local flags”. A comment in macOS’s <termios.h> describes it as a “dumping ground for other state”. So perhaps it should be thought of as “miscellaneous flags”. The other flag fields are c_iflag (input flags), c_oflag (output flags), and c_cflag (control flags), all of which we will have to modify to enable raw mode.

ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary. We use the bitwise-NOT operator (~) on this value to get 11111111111111111111111111110111. We then bitwise-AND this value with the flags field, which forces the fourth bit in the flags field to become 0, and causes every other bit to retain its current value. Flipping bits like this is common in C.

atexit() comes from <stdlib.h>. We use it to register our disableRawMode() function to be called automatically when the program exits, whether it exits by returning from main(), or by calling the exit() function. This way we can ensure we’ll leave the terminal attributes the way we found them when our program exits.

We store the original terminal attributes in a global variable, orig_termios. We assign the orig_termios struct to the raw struct in order to make a copy of it before we start making our changes.

You may notice that leftover input is no longer fed into your shell after the program quits. This is because of the TCSAFLUSH option being passed to tcsetattr() when the program exits. As described earlier, it discards any unread input before applying the changes to the terminal. (Note: This doesn’t happen in Cygwin for some reason, but it won’t matter once we are reading input one byte at a time.)

ICANON comes from <termios.h>. Input flags (the ones in the c_iflag field) generally start with I like ICANON does. However, ICANON is not an input flag, it’s a “local” flag in the c_lflag field. So that’s confusing.

iscntrl() comes from <ctype.h>, and printf() comes from <stdio.h>.

iscntrl() tests whether a character is a control character. Control characters are nonprintable characters that we don’t want to print to the screen. ASCII codes 0–31 are all control characters, and 127 is also a control character. ASCII codes 32–126 are all printable. (Check out the ASCII table to see all of the characters.)

printf() can print multiple representations of a byte. %d tells it to format the byte as a decimal number (its ASCII code), and %c tells it to write out the byte directly, as a character.

You’ll notice a few interesting things:

Arrow keys, Page Up, Page Down, Home, and End all input 3 or 4 bytes to the terminal: 27, '[', and then one or two other characters. This is known as an escape sequence. All escape sequences start with a 27 byte. Pressing Escape sends a single 27 byte as input.
Backspace is byte 127. Delete is a 4-byte escape sequence.
Enter is byte 10, which is a newline character, also known as '\n'.
Ctrl-A is 1, Ctrl-B is 2, Ctrl-C is… oh, that terminates the program, right. But the Ctrl key combinations that do work seem to map the letters A–Z to the codes 1–26.

By default, Ctrl-C sends a SIGINT signal to the current process which causes it to terminate, and Ctrl-Z sends a SIGTSTP signal to the current process which causes it to suspend. Let’s turn off the sending of both of these signals.

ISIG comes from <termios.h>. Like ICANON, it starts with I but isn’t an input flag.

Now Ctrl-C can be read as a 3 byte and Ctrl-Z can be read as a 26 byte.

This also disables Ctrl-Y on macOS, which is like Ctrl-Z except it waits for the program to read input before suspending it.

IXON comes from <termios.h>. The I stands for “input flag” (which it is, unlike the other I flags we’ve seen so far) and XON comes from the names of the two control characters that Ctrl-S and Ctrl-Q produce: XOFF to pause transmission and XON to resume transmission.

Now Ctrl-S can be read as a 19 byte and Ctrl-Q can be read as a 17 byte.


IEXTEN comes from <termios.h>. It is another flag that starts with I but belongs in the c_lflag field.

Ctrl-V can now be read as a 22 byte, and Ctrl-O as a 15 byte.


ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for “carriage return”, and NL stands for “new line”.

Now Ctrl-M is read as a 13 (carriage return), and the Enter key is also read as a 13.