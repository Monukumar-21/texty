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


t turns out that the terminal does a similar translation on the output side. It translates each newline ("\n") we print into a carriage return followed by a newline ("\r\n"). The terminal requires both of these characters in order to start a new line of text. The carriage return moves the cursor back to the beginning of the current line, and the newline moves the cursor down a line, scrolling the screen if necessary. (These two distinct operations originated in the days of typewriters and teletypes.)

We will turn off all output processing features by turning off the OPOST flag. In practice, the "\n" to "\r\n" translation is likely the only output processing feature turned on by default.


OPOST comes from <termios.h>. O means it’s an output flag, and I assume POST stands for “post-processing of output”.

If you run the program now, you’ll see that the newline characters we’re printing are only moving the cursor down, and not to the left side of the screen. To fix that, let’s add carriage returns to our printf() statements.



BRKINT, INPCK, ISTRIP, and CS8 all come from <termios.h>.

This step probably won’t have any observable effect for you, because these flags are either already turned off, or they don’t really apply to modern terminal emulators. But at one time or another, switching them off was considered (by someone) to be part of enabling “raw mode”, so we carry on the tradition (of whoever that someone was) in our program.

As far as I can tell:

When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators.
ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off.
CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off. It sets the character size (CS) to 8 bits per byte. On my system, it’s already set that way.


VMIN and VTIME come from <termios.h>. They are indexes into the c_cc field, which stands for “control characters”, an array of bytes that control various terminal settings.

The VMIN value sets the minimum number of bytes of input needed before read() can return. We set it to 0 so that read() returns as soon as there is any input to be read. The VTIME value sets the maximum amount of time to wait before read() returns. It is in tenths of a second, so we set it to 1/10 of a second, or 100 milliseconds. If read() times out, it will return 0, which makes sense because its usual return value is the number of bytes read.

When you run the program, you can see how often read() times out. If you don’t supply any input, read() returns without setting the c variable, which retains its 0 value and so you see 0s getting printed out. If you type really fast, you can see that read() returns right away after each keypress, so it’s not like you can only read one keypress every tenth of a second.

If you’re using Bash on Windows, you may see that read() still blocks for input. It doesn’t seem to care about the VTIME value. Fortunately, this won’t make too big a difference in our text editor, as we’ll be basically blocking for input anyways.

errno and EAGAIN come from <errno.h>.

tcsetattr(), tcgetattr(), and read() all return -1 on failure, and set the errno value to indicate the error.

In Cygwin, when read() times out it returns -1 with an errno of EAGAIN, instead of just returning 0 like it’s supposed to. To make it work in Cygwin, we won’t treat EAGAIN as an error.

An easy way to make tcgetattr() fail is to give your program a text file or a pipe as the standard input instead of your terminal. To give it a file as standard input, run ./kilo <kilo.c. To give it a pipe, run echo test | ./kilo. Both should result in the same error from tcgetattr(), something like Inappropriate ioctl for device.

The CTRL_KEY macro bitwise-ANDs a character with the value 00011111, in binary. (In C, you generally specify bitmasks using hexadecimal, since C doesn’t have binary literals, and hexadecimal is more concise and readable once you get used to it.) In other words, it sets the upper 3 bits of the character to 0. This mirrors what the Ctrl key does in the terminal: it strips bits 5 and 6 from whatever key you press in combination with Ctrl, and sends that. (By convention, bit numbering starts from 0.) The ASCII character set seems to be designed this way on purpose. (It is also similarly designed so that you can set and clear bit 5 to switch between lowercase and uppercase.)


editorReadKey()’s job is to wait for one keypress, and return it. Later, we’ll expand this function to handle escape sequences, which involves reading multiple bytes that represent a single keypress, as is the case with the arrow keys.

editorProcessKeypress() waits for a keypress, and then handles it. Later, it will map various Ctrl key combinations and other special keys to different editor functions, and insert any alphanumeric and other printable keys’ characters into the text that is being edited.

Note that editorReadKey() belongs in the /*** terminal ***/ section because it deals with low-level terminal input, whereas editorProcessKeypress() belongs in the new /*** input ***/ section because it deals with mapping keys to editor functions at a much higher level.

Now we have vastly simplified main(), and we will try to keep it that way.

write() and STDOUT_FILENO come from <unistd.h>.

The 4 in our write() call means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal. (Try and remember \x1b, we will be using it a lot.) The other three bytes are [2J.

We are writing an escape sequence to the terminal. Escape sequences always start with an escape character (27) followed by a [ character. Escape sequences instruct the terminal to do various text formatting tasks, such as coloring text, moving the cursor around, and clearing parts of the screen.

We are using the J command (Erase In Display) to clear the screen. Escape sequence commands take arguments, which come before the command. In this case the argument is 2, which says to clear the entire screen. <esc>[1J would clear the screen up to where the cursor is, and <esc>[0J would clear the screen from the cursor up to the end of the screen. Also, 0 is the default argument for J, so just <esc>[J by itself would also clear the screen from the cursor to the end.

For our text editor, we will be mostly using VT100 escape sequences, which are supported very widely by modern terminal emulators. See the VT100 User Guide for complete documentation of each escape sequence.

If we wanted to support the maximum number of terminals out there, we could use the ncurses library, which uses the terminfo database to figure out the capabilities of a terminal and what escape sequences to use for that particular terminal.

This escape sequence is only 3 bytes long, and uses the H command (Cursor Position) to position the cursor. The H command actually takes two arguments: the row number and the column number at which to position the cursor. So if you have an 80×24 size terminal and you want the cursor in the center of the screen, you could use the command <esc>[12;40H. (Multiple arguments are separated by a ; character.) The default arguments for H both happen to be 1, so we can leave both arguments out and it will position the cursor at the first row and first column, as if we had sent the <esc>[1;1H command. (Rows and columns are numbered starting at 1, not 0.)


We have two exit points we want to clear the screen at: die(), and when the user presses Ctrl-Q to quit.

We could use atexit() to clear the screen when our program exits, but then the error message printed by die() would get erased right after printing it.

editorDrawRows() will handle drawing each row of the buffer of text being edited. For now it draws a tilde in each row, which means that row is not part of the file and can’t contain any text.

We don’t know the size of the terminal yet, so we don’t know how many rows to draw. For now we just draw 24 rows.

After we’re done drawing, we do another <esc>[H escape sequence to reposition the cursor back up at the top-left corner.

ioctl(), TIOCGWINSZ, and struct winsize come from <sys/ioctl.h>.

On success, ioctl() will place the number of columns wide and the number of rows high the terminal is into the given winsize struct. On failure, ioctl() returns -1. We also check to make sure the values it gave back weren’t 0, because apparently that’s a possible erroneous outcome. If ioctl() failed in either way, we have getWindowSize() report failure by returning -1. If it succeeded, we pass the values back by setting the int references that were passed to the function. (This is a common approach to having functions return multiple values in C. It also allows you to use the return value to indicate success or failure.)

Now let’s add screenrows and screencols to our global editor state, and call getWindowSize() to fill in those values.


The reply is an escape sequence! It’s an escape character (27), followed by a [ character, and then the actual response: 24;80R, or similar. (This escape sequence is documented as Cursor Position Report.)

As before, we’ve inserted a temporary call to editorReadKey() to let us observe our debug output before the screen gets cleared on exit.

(Note: If you’re using Bash on Windows, read() doesn’t time out so you’ll be stuck in an infinite loop. You’ll have to kill the process externally, or exit and reopen the command prompt window.)

We’re going to have to parse this response. But first, let’s read it into a buffer. We’ll keep reading characters until we get to the R character.


When we print out the buffer, we don’t want to print the '\x1b' character, because the terminal would interpret it as an escape sequence and wouldn’t display it. So we skip the first character in buf by passing &buf[1] to printf(). printf() expects strings to end with a 0 byte, so we make sure to assign '\0' to the final byte of buf.

If you run the program, you’ll see we have the response in buf in the form of <esc>[24;80. Let’s parse the two numbers out of there using sscanf():

sscanf() comes from <stdio.h>.

First we make sure it responded with an escape sequence. Then we pass a pointer to the third character of buf to sscanf(), skipping the '\x1b' and '[' characters. So we are passing a string of the form 24;80 to sscanf(). We are also passing it the string %d;%d which tells it to parse two integers separated by a ;, and put the values into the rows and cols variables.

Our fallback method for getting the window size is now complete. You should see that editorDrawRows() prints the correct number of tildes for the height of your terminal.

Now that we know that works, let’s remove the 1 || we put in the if condition temporarily.


It’s not a good idea to make a whole bunch of small write()’s every time we refresh the screen. It would be better to do one big write(), to make sure the whole screen updates at once. Otherwise there could be small unpredictable pauses between write()’s, which would cause an annoying flicker effect.

We want to replace all our write() calls with code that appends the string to a buffer, and then write() this buffer out at the end. Unfortunately, C doesn’t have dynamic strings, so we’ll create our own dynamic string type that supports one operation: appending.

Let’s start by making a new /*** append buffer ***/ section, and defining the abuf struct under it.

An append buffer consists of a pointer to our buffer in memory, and a length. We define an ABUF_INIT constant which represents an empty buffer. This acts as a constructor for our abuf type.

Next, let’s define the abAppend() operation, as well as the abFree() destructor.

realloc() and free() come from <stdlib.h>. memcpy() comes from <string.h>.

To append a string s to an abuf, the first thing we do is make sure we allocate enough memory to hold the new string. We ask realloc() to give us a block of memory that is the size of the current string plus the size of the string we are appending. realloc() will either extend the size of the block of memory we already have allocated, or it will take care of free()ing the current block of memory and allocating a new block of memory somewhere else that is big enough for our new string.

Then we use memcpy() to copy the string s after the end of the current data in the buffer, and we update the pointer and length of the abuf to the new values.

abFree() is a destructor that deallocates the dynamic memory used by an abuf.

Okay, our abuf type is ready to be put to use.

In editorRefreshScreen(), we first initialize a new abuf called ab, by assigning ABUF_INIT to it. We then replace each occurrence of write(STDOUT_FILENO, ...) with abAppend(&ab, ...). We also pass ab into editorDrawRows(), so it too can use abAppend(). Lastly, we write() the buffer’s contents out to standard output, and free the memory used by the abuf.

snprintf() comes from <stdio.h>.

We use the welcome buffer and snprintf() to interpolate our KILO_VERSION string into the welcome message. We also truncate the length of the string in case the terminal is too tiny to fit our welcome message.

