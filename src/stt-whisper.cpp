#include "stt-whisper.h"
#include <whisper.h>
#include <obs-module.h>
#include <stdexcept>

std::mutex WhisperModel::registry_mutex_;
std::map<std::string, WhisperModel*> WhisperModel::registry_;

WhisperModel& WhisperModel::get(const std::string& path) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto& ptr = registry_[path];
    if (!ptr) {
        ptr = new WhisperModel();
        ptr->ctx = whisper_init_from_file(path.c_str());
        if (!ptr->ctx)
            throw std::runtime_error("Failed to load whisper model: " + path);
    }
    ++ptr->ref_count;
    return *ptr;
}

void WhisperModel::release(const std::string& path) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = registry_.find(path);
    if (it == registry_.end()) return;
    if (--it->second->ref_count <= 0) {
        whisper_free(it->second->ctx);
        delete it->second;
        registry_.erase(it);
    }
}

SttWhisper::SttWhisper(Config cfg) : cfg_(std::move(cfg)) {
    model_ = &WhisperModel::get(cfg_.model_path);
}

SttWhisper::~SttWhisper() {
    if (model_) WhisperModel::release(cfg_.model_path);
}

SttResult SttWhisper::transcribe(const float* samples, size_t num_samples) {
    std::lock_guard<std::mutex> lock(model_->mutex);

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language   = cfg_.language.c_str();
    params.n_threads  = cfg_.n_threads;
    params.single_segment = true;
    params.print_realtime = false;
    params.print_progress = false;
    params.print_special  = false;
    params.translate      = false;
    params.no_speech_thold = 1.0f;  // disable VAD filter
    params.logprob_thold   = -1.f; // most permissive logprob

    blog(LOG_INFO, "[SpeechToTelop] whisper_full: %zu samples (%.1fs)",
         num_samples, static_cast<float>(num_samples) / 16000.f);

    int wret = whisper_full(model_->ctx, params, samples, static_cast<int>(num_samples));
    blog(LOG_INFO, "[SpeechToTelop] whisper_full ret=%d", wret);
    if (wret != 0)
        return {"", false};

    std::string text;
    int n = whisper_full_n_segments(model_->ctx);
    blog(LOG_INFO, "[SpeechToTelop] whisper segments: %d", n);
    for (int i = 0; i < n; ++i)
        text += whisper_full_get_segment_text(model_->ctx, i);

    // Strip leading/trailing whitespace
    auto ltrim = [](std::string& s) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
        if (i) s = s.substr(i);
    };
    auto rtrim = [](std::string& s) {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
    };
    ltrim(text); rtrim(text);
    if (text.empty()) return {"", false};

    // Strip leading speaker tag like "(知事) " or "[記者] " that whisper sometimes prepends.
    // Only strip if there's real text after the closing bracket.
    auto strip_leading_tag = [](std::string& s, char open, char close) {
        if (s.empty() || s[0] != open) return;
        size_t close_pos = s.find(close);
        if (close_pos == std::string::npos) return;
        // Make sure there's actual content after the tag
        size_t after = close_pos + 1;
        while (after < s.size() && s[after] == ' ') ++after;
        if (after < s.size())
            s = s.substr(after);
    };
    strip_leading_tag(text, '(', ')');
    strip_leading_tag(text, '[', ']');
    ltrim(text);
    if (text.empty()) return {"", false};
    blog(LOG_INFO, "[SpeechToTelop] after_filter: '%s'", text.c_str());

    // Discard if entire text is wrapped in () or [] — whisper's uncertainty marker
    {
        auto wrapped = [&](char open, char close) {
            return text.size() >= 2 && text.front() == open && text.back() == close;
        };
        if (wrapped('(', ')') || wrapped('[', ']')) return {"", false};

        auto starts = [&](const char* s) { return text.rfind(s, 0) == 0; };
        auto ends   = [&](const char* s) {
            size_t l = std::strlen(s);
            return text.size() >= l && text.compare(text.size() - l, l, s) == 0;
        };
        if (starts("（") && ends("）")) return {"", false};
        if (starts("【") && ends("】")) return {"", false};
        if (starts("「") && ends("」")) return {"", false};
    }

    // Discard known hallucination phrases
    static const char* kHallucinations[] = {
        "ご視聴ありがとうございました",
        "ご覧いただきありがとうございました",
        "チャンネル登録",
        "字幕は自動生成",
    };
    for (const char* h : kHallucinations) {
        if (text.find(h) != std::string::npos)
            return {"", false};
    }

    return {text, true};
}

void SttWhisper::shutdown() {
    if (model_) {
        WhisperModel::release(cfg_.model_path);
        model_ = nullptr;
    }
}
