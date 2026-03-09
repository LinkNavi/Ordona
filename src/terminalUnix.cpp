#include "terminal.h"
#include <cstdio>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig;
static bool saved = false;

void term_enable_raw()
{
    if (!saved) { tcgetattr(STDIN_FILENO, &orig); saved = true; }
    struct termios raw = orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void term_disable_raw()
{
    if (saved) tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

int term_get_cols()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

std::string term_read_key()
{
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return "CTRL_D";

    if (c == 1)   return "CTRL_A";
    if (c == 3)   return "CTRL_C";
    if (c == 4)   return "CTRL_D";
    if (c == 5)   return "CTRL_E";
    if (c == 11)  return "CTRL_K";
    if (c == 12)  return "CTRL_L";
    if (c == 21)  return "CTRL_U";
    if (c == 23)  return "CTRL_W";
    if (c == 9)   return "TAB";
    if (c == 13 || c == 10) return "ENTER";
    if (c == 127 || c == 8) return "BACKSPACE";

    if (c == 27)
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return "ESC";
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return "ESC";

        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                char trail;
                if (read(STDIN_FILENO, &trail, 1) <= 0) return "ESC";
                if (trail == '~')
                {
                    if (seq[1] == '3') return "DEL";
                    if (seq[1] == '1' || seq[1] == '7') return "HOME";
                    if (seq[1] == '4' || seq[1] == '8') return "END";
                }
                return "ESC";
            }
            if (seq[1] == 'A') return "UP";
            if (seq[1] == 'B') return "DOWN";
            if (seq[1] == 'C') return "RIGHT";
            if (seq[1] == 'D') return "LEFT";
            if (seq[1] == 'H') return "HOME";
            if (seq[1] == 'F') return "END";
        }
        if (seq[0] == 'O')
        {
            if (seq[1] == 'H') return "HOME";
            if (seq[1] == 'F') return "END";
        }
        return "ESC";
    }

    // UTF-8 multibyte
    std::string s(1, c);
    if ((c & 0xE0) == 0xC0) { char b; if (read(STDIN_FILENO, &b, 1) > 0) s += b; }
    else if ((c & 0xF0) == 0xE0) { char b[2]; if (read(STDIN_FILENO, b, 2) == 2) s.append(b, 2); }
    else if ((c & 0xF8) == 0xF0) { char b[3]; if (read(STDIN_FILENO, b, 3) == 3) s.append(b, 3); }

    return s;
}

void term_write(const char* s, int len) { write(STDOUT_FILENO, s, len); }
void term_write(const std::string& s)   { write(STDOUT_FILENO, s.data(), s.size()); }

void term_clear_line() { term_write("\x1b[2K\r"); }
void term_move_col(int col)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\x1b[%dG", col);
    term_write(buf, n);
}
void term_cursor_hide() { term_write("\x1b[?25l"); }
void term_cursor_show() { term_write("\x1b[?25h"); }
