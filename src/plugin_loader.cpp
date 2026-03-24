// src/plugin_loader.cpp
#include "plugin_loader.h"
#include "ordona.h"
#include "git.h"
#include "console.h"
#include <filesystem>
#include <vector>
#include <iostream>
#include <unordered_map>

#ifdef _WIN32
  #include <windows.h>
  using LibHandle = HMODULE;
  #define LOAD_LIB(p)   LoadLibraryA(p)
  #define GET_SYM(h, s) GetProcAddress(h, s)
  #define CLOSE_LIB(h)  FreeLibrary(h)
  #define LIB_EXT       ".dll"
#else
  #include <dlfcn.h>
  using LibHandle = void*;
  #define LOAD_LIB(p)   dlopen(p, RTLD_LAZY)
  #define GET_SYM(h, s) dlsym(h, s)
  #define CLOSE_LIB(h)  dlclose(h)
  #define LIB_EXT       ".so"
#endif

namespace fs = std::filesystem;

struct LoadedPlugin {
    LibHandle handle;
    PluginAPI* api;
    std::string name;
};

static std::vector<LoadedPlugin> plugins;
static std::unordered_map<std::string, std::string> claimed_commands;

static void register_commands(PluginAPI* api, const std::string& name)
{
    if (!api->commands) return;
    for (int i = 0; api->commands[i] != nullptr; i++)
    {
        std::string cmd = api->commands[i];
        auto it = claimed_commands.find(cmd);
        if (it != claimed_commands.end())
            std::cerr << "warning: '" << name << "' and '" << it->second
                      << "' both claim '" << cmd << "', " << it->second << " takes priority\n";
        else
            claimed_commands[cmd] = name;
    }
}

static void unregister_commands(PluginAPI* api)
{
    if (!api->commands) return;
    for (int i = 0; api->commands[i] != nullptr; i++)
        claimed_commands.erase(api->commands[i]);
}

static void load_one(const fs::path& path)
{
    LibHandle h = LOAD_LIB(path.string().c_str());
    if (!h) { std::cerr << "plugin load failed: " << path << "\n"; return; }

    using EntryFn = PluginAPI*(*)();
    auto fn = (EntryFn)GET_SYM(h, "ordona_plugin");
    if (!fn) { CLOSE_LIB(h); return; }

    PluginAPI* api = fn();
    if (!api) { CLOSE_LIB(h); return; }

    std::string name = api->name ? api->name : path.stem().string();

    register_commands(api, name);

    plugins.push_back({h, api, name});

    try { if (api->on_init) api->on_init(); }
    catch (...) { std::cerr << "plugin " << name << " crashed in on_init\n"; }

    std::cout << "loaded plugin: " << name;
    if (api->version) std::cout << " v" << api->version;
    std::cout << "\n";
}

void plugins_load()
{
    fs::path dir = fs::path(ordona_dir()) / "plugins";
    if (!fs::exists(dir)) return;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() != LIB_EXT) continue;
        load_one(entry.path());
    }
}

void plugins_unload()
{
    for (auto& p : plugins) {
        try { if (p.api->on_exit) p.api->on_exit(); }
        catch (...) { std::cerr << "plugin " << p.name << " crashed in on_exit\n"; }
        unregister_commands(p.api);
        CLOSE_LIB(p.handle);
    }
    plugins.clear();
    claimed_commands.clear();
}

void plugin_install(std::string repo)
{
    std::string name = repo_name(repo);
    std::string src  = ordona_dir() + "plugins/src/" + name;

    clone_repo(repo);

#ifdef _WIN32
    execute_cmd("cd " + src + " && build.bat");
    fs::path lib_src = src + "/dist/" + name + ".dll";
    fs::path lib_dst = ordona_dir() + "plugins/" + name + ".dll";
#else
    execute_cmd("cd " + src + " && sh build.sh");
    fs::path lib_src = src + "/dist/" + name + ".so";
    fs::path lib_dst = ordona_dir() + "plugins/" + name + ".so";
#endif

    try {
        fs::create_directories(fs::path(lib_dst).parent_path());
        fs::copy_file(lib_src, lib_dst, fs::copy_options::overwrite_existing);
        std::cout << "installed plugin: " << name << "\n";
        load_one(lib_dst);
    } catch (...) {
        std::cerr << "build failed, expected output at: " << lib_src << "\n";
    }
}

void plugin_remove(const std::string& name)
{
    for (auto it = plugins.begin(); it != plugins.end(); ++it)
    {
        if (it->name == name) {
            try { if (it->api->on_exit) it->api->on_exit(); }
            catch (...) {}
            unregister_commands(it->api);
            CLOSE_LIB(it->handle);
            plugins.erase(it);
            break;
        }
    }

    fs::path lib = fs::path(ordona_dir()) / "plugins" / (name + LIB_EXT);
    fs::path src = fs::path(ordona_dir()) / "plugins/src" / name;

    try {
        if (fs::exists(lib)) fs::remove(lib);
        if (fs::exists(src)) fs::remove_all(src);
        std::cout << "removed plugin: " << name << "\n";
    } catch (...) {
        std::cerr << "failed to remove plugin files for: " << name << "\n";
    }
}

void plugin_list()
{
    if (plugins.empty()) { std::cout << "no plugins loaded\n"; return; }
    for (const auto& p : plugins) {
        std::cout << "  " << p.name;
        if (p.api->version) std::cout << " v" << p.api->version;
        if (p.api->commands) {
            std::cout << " [";
            for (int i = 0; p.api->commands[i] != nullptr; i++) {
                if (i) std::cout << ", ";
                std::cout << p.api->commands[i];
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }
}

void plugin_disable(const std::string& name)
{
    fs::path lib      = fs::path(ordona_dir()) / "plugins" / (name + LIB_EXT);
    fs::path disabled = fs::path(ordona_dir()) / "plugins" / (name + LIB_EXT + ".disabled");

    for (auto it = plugins.begin(); it != plugins.end(); ++it)
    {
        if (it->name == name) {
            try { if (it->api->on_exit) it->api->on_exit(); }
            catch (...) {}
            unregister_commands(it->api);
            CLOSE_LIB(it->handle);
            plugins.erase(it);
            break;
        }
    }

    try {
        if (fs::exists(lib)) { fs::rename(lib, disabled); std::cout << "disabled plugin: " << name << "\n"; }
        else std::cerr << "plugin not found: " << name << "\n";
    } catch (...) {
        std::cerr << "failed to disable plugin: " << name << "\n";
    }
}

void plugin_enable(const std::string& name)
{
    fs::path disabled = fs::path(ordona_dir()) / "plugins" / (name + LIB_EXT + ".disabled");
    fs::path lib      = fs::path(ordona_dir()) / "plugins" / (name + LIB_EXT);

    try {
        if (fs::exists(disabled)) {
            fs::rename(disabled, lib);
            load_one(lib);
            std::cout << "enabled plugin: " << name << "\n";
        } else {
            std::cerr << "disabled plugin not found: " << name << "\n";
        }
    } catch (...) {
        std::cerr << "failed to enable plugin: " << name << "\n";
    }
}

bool plugin_needs_update(const std::string& name)
{
    std::string src = ordona_dir() + "plugins/src/" + name;
    if (!fs::exists(src)) return false;
    execute_cmd("cd " + src + " && git fetch");
#ifdef _WIN32
    FILE* f = _popen(("cd " + src + " && git rev-list HEAD..origin/HEAD --count").c_str(), "r");
#else
    FILE* f = popen(("cd " + src + " && git rev-list HEAD..origin/HEAD --count").c_str(), "r");
#endif
    if (!f) return false;
    int count = 0;
    fscanf(f, "%d", &count);
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    return count > 0;
}

void plugin_update(const std::string& name)
{
    std::string src = ordona_dir() + "plugins/src/" + name;
    execute_cmd("cd " + src + " && git pull");
#ifdef _WIN32
    execute_cmd("cd " + src + " && build.bat");
    fs::copy_file(src + "/dist/" + name + ".dll", ordona_dir() + "plugins/" + name + ".dll", fs::copy_options::overwrite_existing);
#else
    execute_cmd("cd " + src + " && sh build.sh");
    fs::copy_file(src + "/dist/" + name + ".so", ordona_dir() + "plugins/" + name + ".so", fs::copy_options::overwrite_existing);
#endif
    std::cout << "updated: " << name << "\n";
}

void plugins_update_all()
{
    fs::path src_dir = fs::path(ordona_dir()) / "plugins/src";
    if (!fs::exists(src_dir)) return;
    for (const auto& entry : fs::directory_iterator(src_dir))
    {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (plugin_needs_update(name)) plugin_update(name);
        else std::cout << name << " is up to date\n";
    }
}

bool plugins_on_command(const std::string& input)
{
    for (auto& p : plugins) {
        if (!p.api->on_command) continue;
        try { if (p.api->on_command(input)) return true; }
        catch (...) { std::cerr << "plugin " << p.name << " crashed in on_command\n"; }
    }
    return false;
}

std::string plugins_on_hint(const std::string& input)
{
    for (auto& p : plugins) {
        if (!p.api->on_hint) continue;
        try {
            std::string h = p.api->on_hint(input);
            if (!h.empty()) return h;
        } catch (...) { std::cerr << "plugin " << p.name << " crashed in on_hint\n"; }
    }
    return "";
}

void plugins_on_cd(const std::string& dir)
{
    for (auto& p : plugins) {
        if (!p.api->on_cd) continue;
        try { p.api->on_cd(dir); }
        catch (...) { std::cerr << "plugin " << p.name << " crashed in on_cd\n"; }
    }
}