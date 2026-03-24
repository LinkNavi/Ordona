// include/plugin_loader.h
#pragma once
#include "plugin.h"
#include <string>

void plugins_load();
void plugins_unload();
void plugin_install(std::string repo);
void plugin_remove(const std::string& name);
void plugin_list();
void plugin_disable(const std::string& name);
void plugin_enable(const std::string& name);
void plugins_update_all();
bool plugins_on_command(const std::string& input);
std::string plugins_on_hint(const std::string& input);
void plugins_on_cd(const std::string& dir);