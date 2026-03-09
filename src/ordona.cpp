#include "console.h"
#include "ordona.h"
#include "predictor.h"
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <cstring>
namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> aliases;
replxx::Replxx rx;

// default prompt — user can override in ~/.ordonarc or config
std::string prompt_format = "\x01\x1b[34m\x02{cwd}\x01\x1b[0m\x02 \x01\x1b[33m\x02({branch})\x01\x1b[0m\x02 \x01\x1b[32m\x02$>\x01\x1b[0m\x02 ";

// ── signal ────────────────────────────────────────────────────

// ── paths ─────────────────────────────────────────────────────

static std::string ordona_dir()
{
#ifdef _WIN32
    const char* base = getenv("APPDATA");
    return base ? std::string(base) + "/Ordona/" : "";
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg) return std::string(xdg) + "/Ordona/";
    const char* home = getenv("HOME");
    return home ? std::string(home) + "/.config/Ordona/" : "";
#endif
}

std::string get_config_path()    { return ordona_dir() + "ordona.conf";   }
std::string get_history_path()   { return ordona_dir() + "ordona_history"; }
std::string get_alias_path()     { return ordona_dir() + "alias";          }
std::string get_predictor_path() { return ordona_dir() + "ordona_ngram";   }

std::string get_rc_path()
{
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
#else
    const char* home = getenv("HOME");
#endif
    return home ? std::string(home) + "/.ordonarc" : "";
}

bool config_exists() { return fs::exists(get_config_path()); }

// ── file util ─────────────────────────────────────────────────

bool writeToFile(const std::string& filePath, const std::string& content)
{
    try {
        fs::path p(filePath);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream f(p);
        if (!f.is_open()) return false;
        f << content;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return false;
    }
}

// ── expansion ─────────────────────────────────────────────────

std::string expand_tilde(const std::string& path)
{
    if (path.empty() || path[0] != '~') return path;
    const char* home = getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

std::string resolve_env_vars(const std::string& input)
{
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '$' && i + 1 < input.size())
        {
            size_t start = i + 1;
            size_t end   = start;
            while (end < input.size() && (isalnum(input[end]) || input[end] == '_'))
                ++end;
            std::string var = input.substr(start, end - start);
            const char* val = getenv(var.c_str());
            if (val) out += val;
            i = end - 1;
        }
        else out += input[i];
    }
    return out;
}

std::string resolve_aliases(const std::string& input)
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
    FILE* f = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!f) return "";
    char buf[128] = {};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    std::string branch(buf);
    if (!branch.empty() && branch.back() == '\n') branch.pop_back();
    return branch;
}

// Wrap \x1b[...m sequences with \x01..\x02 so replxx knows they're non-printing
static std::string wrap_ansi(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i+1] == '[')
        {
            if (i > 0 && s[i-1] == '\x01') { out += s[i]; continue; }
            out += '\x01';
            while (i < s.size() && s[i] != 'm') out += s[i++];
            out += 'm';
            out += '\x02';
        }
        else out += s[i];
    }
    return out;
}

// Replace {token} placeholders in prompt_format
static std::string make_prompt()
{
    // gather values
    std::string cwd;
    try {
        cwd = fs::current_path().string();
        const char* home = getenv("HOME");
        if (home && cwd.rfind(home, 0) == 0)
            cwd = "~" + cwd.substr(strlen(home));
    } catch (...) { cwd = "?"; }

    std::string branch = git_branch();

    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));

    const char* user = getenv("USER");
    if (!user) user = getenv("USERNAME");
    if (!user) user = "";

    // time
    time_t now = time(nullptr);
    char timebuf[16] = {};
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&now));

    // replace tokens
    auto replace = [](std::string s, const std::string& tok, const std::string& val) {
        size_t pos;
        while ((pos = s.find(tok)) != std::string::npos)
            s.replace(pos, tok.size(), val);
        return s;
    };

    std::string out = prompt_format;
    out = replace(out, "{cwd}",    cwd);
    out = replace(out, "{branch}", branch.empty() ? "" : branch);
    out = replace(out, "{user}",   user);
    out = replace(out, "{host}",   hostname);
    out = replace(out, "{time}",   timebuf);

    // strip {branch} wrapper if no branch (e.g. user wrote "({branch})" outside a git repo)
    // if branch is empty, also remove surrounding parens if they were wrapping it
    if (branch.empty())
    {
        // clean up lone "()" left behind
        size_t pos;
        while ((pos = out.find("()")) != std::string::npos)
            out.erase(pos, 2);
        // clean up double spaces
        while ((pos = out.find("  ")) != std::string::npos)
            out.replace(pos, 2, " ");
    }

    return wrap_ansi(out);
}

// ── built-ins ─────────────────────────────────────────────────

static bool handle_builtin(const std::string& input)
{
    std::istringstream ss(input);
    std::string cmd;
    ss >> cmd;

    if (cmd == "cd")
    {
        std::string path;
        ss >> path;
        if (path.empty()) {
            const char* home = getenv("HOME");
            if (home) chdir(home);
        } else {
            path = expand_tilde(path);
            if (chdir(path.c_str()) != 0)
                std::cerr << "cd: " << path << ": No such file or directory\n";
        }
        return true;
    }

    if (cmd == "export")
    {
        std::string pair;
        ss >> pair;
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
            setenv(pair.substr(0, eq).c_str(), pair.substr(eq + 1).c_str(), 1);
        return true;
    }

    if (cmd == "prompt")
    {
        std::string fmt;
        std::getline(ss, fmt);
        if (!fmt.empty() && fmt[0] == ' ') fmt = fmt.substr(1);
        if (!fmt.empty()) prompt_format = fmt;
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
    if (value.empty()) return;
    value = value.substr(1);
    aliases[name] = value;
}

void save_aliases()
{
    std::ofstream f(get_alias_path());
    if (!f.is_open()) return;
    for (const auto& [name, value] : aliases)
        f << name << '=' << value << '\n';
}

void load_aliases()
{
    std::ifstream f(get_alias_path());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line))
    {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        aliases[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

// ── rc file ───────────────────────────────────────────────────
// ~/.ordonarc supports: alias, export, prompt, and any shell command

void load_rc()
{
    std::string path = get_rc_path();
    if (path.empty() || !fs::exists(path)) return;

    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        // skip blank lines and comments
        if (line.empty() || line[0] == '#') continue;
        take_input(line);
    }
}

// ── config file ───────────────────────────────────────────────
// simple key=value, currently supports: prompt

void load_config()
{
    std::ifstream f(get_config_path());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "prompt") prompt_format = val;
    }
}

// ── hint callback ─────────────────────────────────────────────

static replxx::Replxx::hints_t hint_callback(
    const std::string& input, int& ctx_len, replxx::Replxx::Color& color)
{
    replxx::Replxx::hints_t hints;
    if (input.empty()) return hints;
    std::string suggestion = predictor_suggest(input);
    if (!suggestion.empty())
    {
        color = replxx::Replxx::Color::GRAY;
        hints.emplace_back(suggestion);
    }
    ctx_len = static_cast<int>(input.size());
    return hints;
}

// ── readline ──────────────────────────────────────────────────

std::string read_line()
{
    const char* buf = rx.input(make_prompt());
    if (!buf) suicide();
    std::string input(buf);
    if (!input.empty()) rx.history_add(input);
    return input;
}

// ── lifecycle ─────────────────────────────────────────────────

void suicide()
{
    rx.history_save(get_history_path());
    predictor_save(get_predictor_path());
    save_aliases();
    disable_raw_mode();
    std::cout << "\r\nBye!\n";
    exit(0);
}

void init()
{
    load_aliases();
    rx.set_hint_callback(hint_callback);
    rx.set_word_break_characters(" \t\n\"\\'`@$><=;|&{(");
    rx.set_max_history_size(1000);
    rx.history_load(get_history_path());
    predictor_load(get_predictor_path());

    // Ctrl+C clears the line instead of exiting
    rx.bind_key(replxx::Replxx::KEY::control('C'), [&](char32_t code) -> replxx::Replxx::ACTION_RESULT {
        return rx.invoke(replxx::Replxx::ACTION::CLEAR_SELF, code);
    });

    if (!config_exists())
        writeToFile(get_config_path(), "# Ordona config\n# prompt={cwd} ({branch}) $>\n");
    else
        load_config();

    load_rc();
}


void draw_prompt() {}

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
    else
    {
        input = resolve_aliases(input);
        if (!handle_builtin(input))
        {
            predictor_train(input);
            execute_cmd(input);
        }
    }
}
