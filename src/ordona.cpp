#include <iostream>
#include "console.h"
#include <string>
void draw_prompt()
{
    std::cout<<"\r$> ";
}

void take_input(std::string input)
{
    if (input == "test")
    {
        std::cout << "test" << std::endl;
    }
    else
    {
        execute_cmd(input);
    }
}
