// include/plugin.h
#pragma once
#include <string>

struct PluginAPI {
    const char* name      = nullptr;
    const char* version   = nullptr;
    const char** commands = nullptr; // null-terminated list, e.g. {"ls", "cat", nullptr}
    void (*on_init)()                                = nullptr;
    void (*on_exit)()                                = nullptr;
    bool (*on_command)(const std::string& input)     = nullptr;
    std::string (*on_hint)(const std::string& input) = nullptr;
    void (*on_cd)(const std::string& newdir)         = nullptr;
};

extern "C" PluginAPI* ordona_plugin();