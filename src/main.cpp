#include "console.h"
#include "ordona.h"
#include <iostream>
#include <string>

int main() {
    setup_raw_mode();

    while (true) {
        draw_prompt();

        std::string input = read_line();
        if (input.empty()) continue;



        take_input(input);
    }

    return 0;
}