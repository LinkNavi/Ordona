#include <termios.h>
#include <unistd.h>
#include "console.h"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <string>
#include <iostream>

struct termios orig_termios;

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void execute_cmd(const std::string& cmd)
{
    disable_raw_mode();
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
    enable_raw_mode();
}
