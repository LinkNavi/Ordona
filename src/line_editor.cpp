#include "line_editor.h"
#include "terminal.h"
#include <algorithm>
#include <fstream>

// Count visible characters (skip ANSI escape sequences)
static int visible_len(const std::string& s)
{
    int len = 0;
    bool in_esc = false;
    for (char c : s)
    {
        if (in_esc) { if (c == 'm') in_esc = false; continue; }
        if (c == '\x1b') { in_esc = true; continue; }
        if ((unsigned char)c >= 0x80 && (unsigned char)c < 0xC0) continue; // UTF-8 continuation
        ++len;
    }
    return len;
}

int LineEditor::prompt_visible_len() const { return visible_len(prompt_); }

void LineEditor::refresh()
{
    term_cursor_hide();
    term_clear_line();
    term_write(prompt_);
    term_write(buf_);

    // Draw hint in gray after buffer
    std::string hint;
    if (hint_fn_) hint = hint_fn_(buf_);
    if (!hint.empty() && hint.size() > buf_.size())
    {
        std::string ghost = hint.substr(buf_.size());
        term_write("\x1b[90m");
        term_write(ghost);
        term_write("\x1b[0m");
    }

    // Position cursor
    int cursor_col = prompt_visible_len() + pos_ + 1;
    term_move_col(cursor_col);
    term_cursor_show();
}

void LineEditor::history_add(const std::string& line)
{
    if (line.empty()) return;
    if (!history_.empty() && history_.back() == line) return;
    history_.push_back(line);
    if ((int)history_.size() > max_hist_)
        history_.erase(history_.begin());
}

void LineEditor::history_load(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty()) history_.push_back(line);
}

void LineEditor::history_save(const std::string& path)
{
    std::ofstream f(path);
    if (!f.is_open()) return;
    for (const auto& h : history_) f << h << '\n';
}

std::string LineEditor::readline(const std::string& prompt, bool& eof)
{
    prompt_ = prompt;
    buf_.clear();
    pos_ = 0;
    eof = false;
    hist_idx_ = (int)history_.size();
    saved_buf_.clear();

    term_enable_raw();
    refresh();

    while (true)
    {
        std::string key = term_read_key();

        if (key == "ENTER")
        {
            term_write("\r\n");
            term_disable_raw();
            return buf_;
        }
        if (key == "CTRL_C")
        {
            buf_.clear();
            pos_ = 0;
            term_write("^C\r\n");
            term_disable_raw();
            return "";
        }
        if (key == "CTRL_D")
        {
            if (buf_.empty()) {
                term_write("\r\n");
                term_disable_raw();
                eof = true;
                return "";
            }
            // delete char under cursor
            if (pos_ < (int)buf_.size()) buf_.erase(pos_, 1);
        }
        else if (key == "BACKSPACE")
        {
            if (pos_ > 0) { buf_.erase(--pos_, 1); }
        }
        else if (key == "DEL")
        {
            if (pos_ < (int)buf_.size()) buf_.erase(pos_, 1);
        }
        else if (key == "LEFT")
        {
            if (pos_ > 0) --pos_;
        }
        else if (key == "RIGHT")
        {
            if (pos_ < (int)buf_.size()) ++pos_;
        }
        else if (key == "HOME" || key == "CTRL_A")
        {
            pos_ = 0;
        }
        else if (key == "END" || key == "CTRL_E")
        {
            pos_ = (int)buf_.size();
        }
        else if (key == "CTRL_K")
        {
            buf_.erase(pos_);
        }
        else if (key == "CTRL_U")
        {
            buf_.erase(0, pos_);
            pos_ = 0;
        }
        else if (key == "CTRL_W")
        {
            // delete word backwards
            int end = pos_;
            while (pos_ > 0 && buf_[pos_ - 1] == ' ') --pos_;
            while (pos_ > 0 && buf_[pos_ - 1] != ' ') --pos_;
            buf_.erase(pos_, end - pos_);
        }
        else if (key == "CTRL_L")
        {
            term_write("\x1b[2J\x1b[H");
        }
        else if (key == "UP")
        {
            if (hist_idx_ > 0)
            {
                if (hist_idx_ == (int)history_.size()) saved_buf_ = buf_;
                --hist_idx_;
                buf_ = history_[hist_idx_];
                pos_ = (int)buf_.size();
            }
        }
        else if (key == "DOWN")
        {
            if (hist_idx_ < (int)history_.size())
            {
                ++hist_idx_;
                buf_ = (hist_idx_ == (int)history_.size()) ? saved_buf_ : history_[hist_idx_];
                pos_ = (int)buf_.size();
            }
        }
        else if (key == "TAB")
        {
            // Accept hint
            if (hint_fn_)
            {
                std::string hint = hint_fn_(buf_);
                if (!hint.empty() && hint.size() > buf_.size())
                {
                    buf_ = hint;
                    pos_ = (int)buf_.size();
                }
            }
        }
        else if (key.size() >= 1 && key[0] >= 32)
        {
            buf_.insert(pos_, key);
            pos_ += (int)key.size();
        }

        refresh();
    }
}
