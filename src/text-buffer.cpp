#include "text-buffer.h"

static const std::string kBoundaryChars = "。！？.!?";

TextBuffer::TextBuffer(int timeout_ms) : timeout_ms_(timeout_ms) {}

void TextBuffer::push(const std::string& fragment) {
    accum_ += fragment;
    has_content_ = !accum_.empty();
    last_push_ = std::chrono::steady_clock::now();
}

std::string TextBuffer::try_pop() {
    if (!has_content_) return "";

    if (has_sentence_boundary()) {
        std::string result = std::move(accum_);
        accum_.clear();
        has_content_ = false;
        return result;
    }

    auto elapsed = std::chrono::steady_clock::now() - last_push_;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms_) {
        std::string result = std::move(accum_);
        accum_.clear();
        has_content_ = false;
        return result;
    }

    return "";
}

void TextBuffer::clear() {
    accum_.clear();
    has_content_ = false;
}

bool TextBuffer::has_sentence_boundary() const {
    if (accum_.empty()) return false;
    // Check last UTF-8 character against boundary set.
    // Japanese punctuation is 3-byte UTF-8; ASCII punctuation is 1-byte.
    for (const char* p = accum_.c_str() + accum_.size(); p > accum_.c_str(); ) {
        unsigned char c = static_cast<unsigned char>(*(--p));
        if ((c & 0x80) == 0) {
            // ASCII
            return kBoundaryChars.find(c) != std::string::npos;
        }
        if ((c & 0xC0) != 0x80) {
            // Start of multi-byte sequence — extract the full char
            size_t char_start = p - accum_.c_str();
            std::string last_char = accum_.substr(char_start);
            return kBoundaryChars.find(last_char) != std::string::npos;
        }
    }
    return false;
}
