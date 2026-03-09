#pragma once
#include <string>
#include <vector>
#include <functional>

class LineEditor {
public:
    using HintFunc = std::function<std::string(const std::string&)>;

    void set_hint_callback(HintFunc fn) { hint_fn_ = std::move(fn); }
    void history_add(const std::string& line);
    void history_load(const std::string& path);
    void history_save(const std::string& path);
    void set_max_history(int n) { max_hist_ = n; }

    // Returns the edited line. Returns "" and sets eof=true on Ctrl-D/EOF.
    std::string readline(const std::string& prompt, bool& eof);

private:
    void refresh();
    int  prompt_visible_len() const;

    std::string prompt_;
    std::string buf_;
    int pos_ = 0;

    std::vector<std::string> history_;
    int hist_idx_ = 0;
    int max_hist_ = 1000;
    std::string saved_buf_;

    HintFunc hint_fn_;
};
