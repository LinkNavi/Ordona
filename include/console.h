#pragma once
#include <string>

void enable_raw_mode();
void disable_raw_mode();
void execute_cmd(const std::string& cmd);
