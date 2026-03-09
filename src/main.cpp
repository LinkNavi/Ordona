#include "console.h"
#include "ordona.h"
#include <iostream>
#include <string>

int main() {
    enable_raw_mode();

    while (true) {


        std::string input = read_line();
        if (input.empty()) continue;



        take_input(input);
    }

    return 0;
}
