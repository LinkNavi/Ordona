#include "console.h"
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static struct termios cooked;
static bool cooked_saved = false;

void enable_raw_mode() {}
void disable_raw_mode() {}

void execute_cmd(const std::string& cmd)
{
    // save current terminal state and restore cooked mode for child
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    if (!cooked_saved)
    {
        cooked = current;
        cooked_saved = true;
    }
    struct termios cook = current;
    cook.c_lflag |= (ECHO | ICANON | ISIG | IEXTEN);
    cook.c_iflag |= (IXON | ICRNL);
    cook.c_oflag |= OPOST;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &cook);

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

    // restore terminal state replxx expects
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &current);
    write(STDOUT_FILENO, "\r", 1);
}
