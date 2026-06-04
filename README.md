# Texty – A Minimalist Terminal Text Editor

Texty is a tiny, hackable text editor for the terminal, written in C.  
It’s inspired by the classic ["Build Your Own Text Editor"](https://viewsourcecode.org/snaptoken/kilo/) tutorial, 
with fixes and a few extra conveniences added.

## ✨ Features

- **Syntax‑free editing** – just plain text, your way.
- **Full keyboard navigation** – arrows, Home, End, Page Up/Down.
- **Insert & delete** – characters, lines, and automatic line splitting/joining.
- **Tab expansion** – tabs are displayed as spaces (tab stop configurable at compile time).
- **Scroll support** – vertical and horizontal scrolling for large files and long lines.
- **File handling** – open existing files or **create new ones** when the file doesn’t exist.
- **Save** – `Ctrl+S` writes your changes back to disk.
- **Status bar** – shows filename, line count, and cursor position.
- **Message bar** – transient status messages (save confirmations, tips).
- **Clean exit** – `Ctrl+Q` restores the terminal and quits.

## 🛠️ Installation

### Prerequisites
- A Unix‑like operating system (Linux, macOS, WSL, etc.)
- GCC or Clang with C99 support
- `make` (optional)

### Build from source

```bash
git clone https://github.com/Monukumar-21/texty.git
cd texty
make
The binary will be ./texty. You can move it to a directory in your PATH if you like.

Manual compilation (no Makefile)
bash
gcc -Wall -Wextra -std=c99 -O2 -Wno-unused-result -o texty texty.c
🚀 Usage
bash
./texty [filename]
Without a filename – starts with an empty buffer. You can save later with Ctrl+S (but you’ll need to set a filename first – currently, you must open with a name).

With an existing file – opens it for editing.

With a new filename – if the file doesn’t exist, Texty creates an empty buffer. When you save (Ctrl+S), the file is created on disk.

🎮 Key Bindings
Key	Action
Ctrl+Q	Quit (clears screen)
Ctrl+S	Save current file
Arrows	Move cursor
Home / End	Jump to start/end of line
Page Up / Down	Scroll by one screenful
Backspace	Delete character before cursor
Delete	Delete character under cursor
Enter	Insert newline / split line
Tab	Insert a tab character (displayed as spaces)
Printable chars	Insert at cursor
⚙️ Customization
You can easily tweak the editor by editing the source:

Tab size – change TEXTY_TAB_STOP at the top of texty.c.

Keybindings – modify the editorProcessKeypress() function.

Syntax highlighting – extend editorUpdateRow() to add colour codes.

Line numbers – add a flag and draw them in editorDrawRows().

Search – implement Ctrl+F with an input prompt.

Multiple buffers – turn E.row into a linked list of files.

The code is heavily commented and designed to be easily extended.
Check out the Build Your Own Text Editor tutorial for more ideas.

📁 Project Structure
text
texty.c         – the whole editor (single file)
Makefile        – optional build automation
README.md       – this file
📄 License
This project is released under the MIT License. Feel free to use, modify, and distribute it.
See the original tutorial for the base code’s licensing.

Happy hacking!
If you have questions or want to share your customised version, open an issue or a PR.
