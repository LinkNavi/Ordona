#include <windows.h>
#include "console.h"
DWORD orig_mode;

void enable_raw_mode() {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(h, &orig_mode);

    DWORD raw = orig_mode;
    raw &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
             ENABLE_PROCESSED_INPUT);
    raw |= ENABLE_VIRTUAL_TERMINAL_INPUT; // ANSI escape support

    SetConsoleMode(h, raw);
}

void disable_raw_mode() {
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_mode);
}