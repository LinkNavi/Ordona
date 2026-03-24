#include "console.h"
#include "ordona.h"
#include "predictor.h"
#include "terminal.h"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "plugin_loader.h"
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <direct.h>
#endif
namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> aliases;
LineEditor editor;

std::string prompt_format = "\x1b[34m{cwd}\x1b[0m \x1b[33m({branch})\x1b[0m \x1b[32m$>\x1b[0m ";

// ── paths ─────────────────────────────────────────────────────

std::string ordona_dir()
{
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    return base ? std::string(base) + "/Ordona/" : "";
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg)
        return std::string(xdg) + "/Ordona/";
    const char *home = getenv("HOME");
    return home ? std::string(home) + "/.config/Ordona/" : "";
#endif
}

std::string get_config_path() { return ordona_dir() + "ordona.conf"; }
std::string get_history_path() { return ordona_dir() + "ordona_history"; }
std::string get_alias_path() { return ordona_dir() + "alias"; }
std::string get_predictor_path() { return ordona_dir() + "ordona_ngram"; }

std::string get_rc_path()
{
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif
    return home ? std::string(home) + "/.ordonarc" : "";
}

bool config_exists() { return fs::exists(get_config_path()); }

// ── file util ─────────────────────────────────────────────────

bool writeToFile(const std::string &filePath, const std::string &content)
{
    try
    {
        fs::path p(filePath);
        if (p.has_parent_path())
            fs::create_directories(p.parent_path());
        std::ofstream f(p);
        if (!f.is_open())
            return false;
        f << content;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return false;
    }
}

// ── expansion ─────────────────────────────────────────────────

std::string expand_tilde(const std::string &path)
{
    if (path.empty() || path[0] != '~')
        return path;
    const char *home = getenv("HOME");
    if (!home)
        return path;
    return std::string(home) + path.substr(1);
}

std::string resolve_env_vars(const std::string &input)
{
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '$' && i + 1 < input.size())
        {
            size_t start = i + 1, end = start;
            while (end < input.size() && (isalnum(input[end]) || input[end] == '_'))
                ++end;
            std::string var = input.substr(start, end - start);
            const char *val = getenv(var.c_str());
            if (val)
                out += val;
            i = end - 1;
        }
        else
            out += input[i];
    }
    return out;
}

std::string resolve_aliases(const std::string &input)
{
    std::istringstream ss(input);
    std::string first, rest;
    ss >> first;
    std::getline(ss, rest);
    auto it = aliases.find(first);
    if (it != aliases.end())
        return it->second + rest;
    return input;
}

// ── prompt ────────────────────────────────────────────────────

static std::string git_branch()
{
#ifdef _WIN32
    FILE *f = _popen("git rev-parse --abbrev-ref HEAD 2>nul", "r");
#else
    FILE *f = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
#endif
    if (!f)
        return "";
    char buf[128] = {};
    fgets(buf, sizeof(buf), f);
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    std::string branch(buf);
    if (!branch.empty() && branch.back() == '\n')
        branch.pop_back();
    return branch;
}

static std::string make_prompt()
{
    std::string cwd;
    try
    {
        cwd = fs::current_path().string();
        const char *home = getenv("HOME");
        if (home && cwd.rfind(home, 0) == 0)
            cwd = "~" + cwd.substr(strlen(home));
    }
    catch (...)
    {
        cwd = "?";
    }

    std::string branch = git_branch();

    char hostname[256] = {};
#ifdef _WIN32
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
#else
    gethostname(hostname, sizeof(hostname));
#endif

    const char *user = getenv("USER");
    if (!user)
        user = getenv("USERNAME");
    if (!user)
        user = "";

    time_t now = time(nullptr);
    char timebuf[16] = {};
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&now));

    auto replace = [](std::string s, const std::string &tok, const std::string &val)
    {
        size_t pos;
        while ((pos = s.find(tok)) != std::string::npos)
            s.replace(pos, tok.size(), val);
        return s;
    };

    std::string out = prompt_format;
    out = replace(out, "{cwd}", cwd);
    out = replace(out, "{branch}", branch.empty() ? "" : branch);
    out = replace(out, "{user}", user);
    out = replace(out, "{host}", hostname);
    out = replace(out, "{time}", timebuf);

    if (branch.empty())
    {
        size_t pos;
        while ((pos = out.find("()")) != std::string::npos)
            out.erase(pos, 2);
        while ((pos = out.find("  ")) != std::string::npos)
            out.replace(pos, 2, " ");
    }

    return out;
}

// ── built-ins ─────────────────────────────────────────────────

static bool handle_builtin(const std::string &input)
{
    std::istringstream ss(input);
    std::string cmd;
    ss >> cmd;

    if (cmd == "cd")
    {
        std::string path;
        ss >> path;
        if (path.empty())
        {
            const char *home = getenv("USERPROFILE");
#ifndef _WIN32
            if (!home)
                home = getenv("HOME");
#endif
            if (home)
            {
#ifdef _WIN32
                _chdir(home);
#else
                chdir(home);
#endif
                plugins_on_cd(home); // use home here
            }
        }
        else
        {
            path = expand_tilde(path);
#ifdef _WIN32
            if (_chdir(path.c_str()) != 0)
#else
            if (chdir(path.c_str()) != 0)
#endif
                std::cerr << "cd: " << path << ": No such file or directory\n";
            else
                plugins_on_cd(path); // only call on success
        }
        return true;
    }
    if (cmd == "export")
    {
        std::string pair;
        ss >> pair;
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
        {
#ifdef _WIN32
            _putenv_s(pair.substr(0, eq).c_str(), pair.substr(eq + 1).c_str());
#else
            setenv(pair.substr(0, eq).c_str(), pair.substr(eq + 1).c_str(), 1);
#endif
        }
        return true;
    }
    if (cmd == "prompt")
    {
        std::string fmt;
        std::getline(ss, fmt);
        if (!fmt.empty() && fmt[0] == ' ')
            fmt = fmt.substr(1);
        if (!fmt.empty())
            prompt_format = fmt;
        return true;
    }
    return false;
}

// ── aliases ───────────────────────────────────────────────────

void make_alias(std::string input)
{
    std::istringstream ss(input);
    std::string cmd, name, value;
    ss >> cmd >> name;
    std::getline(ss, value);
    if (value.empty())
        return;
    value = value.substr(1);
    aliases[name] = value;
}

void save_aliases()
{
    std::ofstream f(get_alias_path());
    if (!f.is_open())
        return;
    for (const auto &[name, value] : aliases)
        f << name << '=' << value << '\n';
}

void load_aliases()
{
    std::ifstream f(get_alias_path());
    if (!f.is_open())
        return;
    std::string line;
    while (std::getline(f, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        aliases[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

// ── rc / config ───────────────────────────────────────────────

void load_rc()
{
    std::string path = get_rc_path();
    if (path.empty() || !fs::exists(path))
        return;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        
        take_input(line);
    }
}

void load_config()
{
    std::ifstream f(get_config_path());
    if (!f.is_open())
        return;
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "prompt")
            prompt_format = val;
    }
}

// ── readline ──────────────────────────────────────────────────

std::string read_line()
{
    bool eof = false;
    std::string input = editor.readline(make_prompt(), eof);
    if (eof)
        suicide();
    if (!input.empty())
        editor.history_add(input);
    return input;
}

// ── lifecycle ─────────────────────────────────────────────────

void suicide()
{
    editor.history_save(get_history_path());
    predictor_save(get_predictor_path());
    save_aliases();
    term_disable_raw();
    plugins_unload();
    std::cout << "\r\nBye!\n";
    exit(0);
}

void init()
{
    load_aliases();

    editor.set_hint_callback([](const std::string &input) -> std::string
                             {
        std::string h = plugins_on_hint(input);
        if (!h.empty()) return h;
        return predictor_suggest(input); });
    editor.set_max_history(1000);
    editor.history_load(get_history_path());
    predictor_load(get_predictor_path());
    plugins_load();
    if (!config_exists())
        writeToFile(get_config_path(), "# Ordona config\n# prompt={cwd} ({branch}) $>\n");
    else
        load_config();

    load_rc();
}

void draw_prompt()
{
}

// ── take_input ────────────────────────────────────────────────

void take_input(std::string input)
{
    input = expand_tilde(input);
    input = resolve_env_vars(input);

    if (input == "test")
        std::cout << "test\n";
    else if (input == "exit" || input == "quit")
        suicide();
    else if (input.rfind("alias", 0) == 0)
        make_alias(input);
    else if (input.rfind("plugin ", 0) == 0)
    {
        std::istringstream ss(input);
        std::string _, subcmd, arg;
        ss >> _ >> subcmd >> arg;

        if (subcmd == "install")
            plugin_install(arg);
        else if (subcmd == "remove")
            plugin_remove(arg);
        else if (subcmd == "enable")
            plugin_enable(arg);
        else if (subcmd == "disable")
            plugin_disable(arg);
        else if (subcmd == "update")
            plugins_update_all();
        else if (subcmd == "list")
            plugin_list();
        else
            std::cerr << "usage: plugin install/remove/enable/disable/update/list\n";
    }
    else
    {
        input = resolve_aliases(input);
        if (plugins_on_command(input))
            return;
        if (!handle_builtin(input))
        {
            predictor_train(input);
            execute_cmd(input);
        }
    }
}
