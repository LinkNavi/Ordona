#include "console.h"
#include "terminal.h"
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

void execute_cmd(const std::string& cmd)
{
    // Already in raw mode from line editor, but we disable it before fork
    // so the child gets a normal terminal
    term_disable_raw();

    pid_t pid = fork();
    if (pid == 0)
    {
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
    }

    // raw mode re-enabled by next readline() call
}
