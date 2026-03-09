#include "console.h"
#include "terminal.h"
#include <windows.h>
#include <string>

void execute_cmd(const std::string& cmd)
{
    term_disable_raw();

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
