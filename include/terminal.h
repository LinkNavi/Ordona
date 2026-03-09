#pragma once
#include <string>

void term_enable_raw();
void term_disable_raw();
int  term_get_cols();

// Read a single key/escape sequence. Returns:
//   printable char as single char string
//   special keys as names: "UP","DOWN","LEFT","RIGHT","HOME","END","DEL","TAB","ENTER","BACKSPACE","CTRL_C","CTRL_D","CTRL_L","CTRL_A","CTRL_E","CTRL_K","CTRL_U","CTRL_W"
std::string term_read_key();

// Low-level write
void term_write(const char* s, int len);
void term_write(const std::string& s);

// Cursor/screen
void term_clear_line();       // clear entire current line
void term_move_col(int col);  // move cursor to column (1-based)
void term_cursor_hide();
void term_cursor_show();
