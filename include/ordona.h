#include <string>

#include <readline/readline.h>
#include <readline/history.h>

std::string read_line()
{
    char* buf = readline("$> ");
    if (!buf) return "";
    std::string input(buf);
    if (!input.empty()) add_history(buf);
    free(buf);
    return input;
}
void draw_prompt();
void take_input(std::string);


