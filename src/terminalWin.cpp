#include "terminal.h"
#include <windows.h>
#include <cstdio>

static DWORD orig_mode = 0;
static bool saved = false;

void term_enable_raw()
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (!saved) { GetConsoleMode(h, &orig_mode); saved = true; }
    DWORD raw = orig_mode;
    raw &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    raw &= ~ENABLE_VIRTUAL_TERMINAL_INPUT; // remove this
    SetConsoleMode(h, raw);

    // only enable VT on output, not input
    HANDLE ho = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD om;
    GetConsoleMode(ho, &om);
    SetConsoleMode(ho, om | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void term_disable_raw()
{
    if (saved) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_mode);
}

int term_get_cols()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
}

std::string term_read_key()
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD n;
    while (true)
    {
        ReadConsoleInputA(h, &rec, 1, &n);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        DWORD ctrl = rec.Event.KeyEvent.dwControlKeyState;
        bool has_ctrl = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

        if (has_ctrl)
        {
            if (vk == 'A') return "CTRL_A";
            if (vk == 'C') return "CTRL_C";
            if (vk == 'D') return "CTRL_D";
            if (vk == 'E') return "CTRL_E";
            if (vk == 'K') return "CTRL_K";
            if (vk == 'L') return "CTRL_L";
            if (vk == 'U') return "CTRL_U";
            if (vk == 'W') return "CTRL_W";
        }

        if (vk == VK_UP)     return "UP";
        if (vk == VK_DOWN)   return "DOWN";
        if (vk == VK_LEFT)   return "LEFT";
        if (vk == VK_RIGHT)  return "RIGHT";
        if (vk == VK_HOME)   return "HOME";
        if (vk == VK_END)    return "END";
        if (vk == VK_DELETE) return "DEL";
        if (vk == VK_TAB)    return "TAB";
        if (vk == VK_RETURN) return "ENTER";
        if (vk == VK_BACK)   return "BACKSPACE";

        if (ch >= 32) return std::string(1, ch);
    }
}

void term_write(const char* s, int len)
{
    DWORD w;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), s, len, &w, NULL);
}
void term_write(const std::string& s) { term_write(s.data(), (int)s.size()); }

void term_clear_line() { term_write("\x1b[2K\r"); }
void term_move_col(int col)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\x1b[%dG", col);
    term_write(buf, n);
}
void term_cursor_hide() { term_write("\x1b[?25l"); }
void term_cursor_show() { term_write("\x1b[?25h"); }
