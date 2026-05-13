#pragma once
#include <chrono>
#include <string>

class TextBuffer {
public:
    explicit TextBuffer(int timeout_ms);

    void push(const std::string& fragment);
    // Returns complete sentence if available (sentence boundary or timeout); else "".
    std::string try_pop();
    void clear();

private:
    bool has_sentence_boundary() const;

    int timeout_ms_;
    std::string accum_;
    std::chrono::steady_clock::time_point last_push_;
    bool has_content_ = false;
};
