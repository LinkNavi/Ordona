#include "predictor.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using FreqMap  = std::unordered_map<std::string, uint32_t>;
using NGramMap = std::unordered_map<std::string, FreqMap>;

static NGramMap unigrams;
static NGramMap bigrams;
static NGramMap trigrams;

static std::vector<std::string> tokenise(const std::string& s)
{
    std::vector<std::string> toks;
    std::istringstream ss(s);
    std::string t;
    while (ss >> t) toks.push_back(t);
    return toks;
}

static std::string best(const FreqMap& fm)
{
    if (fm.empty()) return "";
    return std::max_element(fm.begin(), fm.end(),
        [](const auto& a, const auto& b){ return a.second < b.second; })->first;
}

static void record(NGramMap& map, const std::string& ctx, const std::string& next)
{
    map[ctx][next]++;
}

static std::string encode(const std::string& s)
{
    std::string out;
    for (char c : s)
        c == ' ' ? out += "\\x20" : out += c;
    return out;
}

static std::string decode(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 3 < s.size() && s.substr(i, 4) == "\\x20")
        { out += ' '; i += 3; }
        else out += s[i];
    }
    return out;
}

static void save_map(std::ofstream& f, int order, const NGramMap& map)
{
    for (const auto& [ctx, fm] : map)
        for (const auto& [word, cnt] : fm)
            f << order << ' ' << encode(ctx) << ' ' << encode(word) << ' ' << cnt << '\n';
}

// ── public API ───────────────────────────────────────────────

void predictor_train(const std::string& cmd)
{
    auto toks = tokenise(cmd);
    if (toks.empty()) return;

    for (size_t i = 0; i < toks.size(); ++i)
    {
        record(unigrams, "", toks[i]);

        if (i + 1 < toks.size())
            record(bigrams, toks[i], toks[i + 1]);

        if (i + 2 < toks.size())
            record(trigrams, toks[i] + " " + toks[i + 1], toks[i + 2]);
    }
}

std::string predictor_suggest(const std::string& input)
{
    if (input.empty()) return "";

    auto toks = tokenise(input);
    bool trailing_space = !input.empty() && input.back() == ' ';

    std::string suggestion;

    if (trailing_space || toks.empty())
    {
        std::string next;

        if (toks.size() >= 2)
        {
            std::string ctx = toks[toks.size()-2] + " " + toks[toks.size()-1];
            auto it = trigrams.find(ctx);
            if (it != trigrams.end()) next = best(it->second);
        }
        if (next.empty() && !toks.empty())
        {
            auto it = bigrams.find(toks.back());
            if (it != bigrams.end()) next = best(it->second);
        }
        if (next.empty())
            next = best(unigrams[""]);

        if (!next.empty())
            suggestion = input + next;
    }
    else
    {
        const std::string& partial = toks.back();

        std::string before;
        for (size_t i = 0; i + 1 < toks.size(); ++i)
            before += (i ? " " : "") + toks[i];

        std::string best_completion;
        uint32_t    best_count = 0;

        auto try_map = [&](const NGramMap& map, const std::string& ctx)
        {
            auto it = map.find(ctx);
            if (it == map.end()) return;
            for (const auto& [word, cnt] : it->second)
            {
                if (word.rfind(partial, 0) == 0 && word != partial && cnt > best_count)
                {
                    best_count      = cnt;
                    best_completion = word;
                }
            }
        };

        if (toks.size() >= 2)
            try_map(trigrams, toks[toks.size()-2] + " " + toks[toks.size()-1]);

        if (toks.size() >= 2)
            try_map(bigrams, toks[toks.size()-2]);

        try_map(unigrams, "");

        if (!best_completion.empty())
            suggestion = before + (before.empty() ? "" : " ") + best_completion;
    }

    if (suggestion.size() > input.size() && suggestion.rfind(input, 0) == 0)
        return suggestion;

    return "";
}

void predictor_save(const std::string& path)
{
    std::ofstream f(path);
    if (!f.is_open()) return;
    save_map(f, 1, unigrams);
    save_map(f, 2, bigrams);
    save_map(f, 3, trigrams);
}

void predictor_load(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return;
    int order; std::string ctx, word; uint32_t cnt;
    while (f >> order >> ctx >> word >> cnt)
    {
        ctx  = decode(ctx);
        word = decode(word);
        switch (order)
        {
            case 1: unigrams[ctx][word] = cnt; break;
            case 2: bigrams[ctx][word]  = cnt; break;
            case 3: trigrams[ctx][word] = cnt; break;
        }
    }
}
