#include "console.h"
#include "ordona.h"
#include <iostream>
#include <string>

int main() { 
	init();

    while (true) {
        std::string input = read_line();
        if (input.empty()) continue;
        take_input(input);
    }
    return 0;
}
