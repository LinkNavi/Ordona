#include <windows.h>
#include "console.h"
#include <string>
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



void execute_cmd(const std::string& cmd)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string mutable_cmd = "cmd.exe /c " + cmd;

    if (CreateProcessA(NULL, &mutable_cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
