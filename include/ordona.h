#pragma once
#include "line_editor.h"
#include <string>
#include <unordered_map>

extern LineEditor editor;
extern std::unordered_map<std::string, std::string> aliases;
extern std::string prompt_format;

std::string get_config_path();
std::string get_history_path();
std::string get_alias_path();
std::string get_predictor_path();
std::string get_rc_path();
bool config_exists();
bool writeToFile(const std::string& filePath, const std::string& content);
std::string expand_tilde(const std::string& path);
std::string resolve_env_vars(const std::string& input);
std::string resolve_aliases(const std::string& input);
std::string read_line();
void draw_prompt();
void take_input(std::string input);
void make_alias(std::string input);
void save_aliases();
void load_aliases();
void load_rc();
void load_config();
void init();
void suicide();
