#include "stt-whisper.h"
#include <whisper.h>
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
    params.single_segment = false;
    params.print_realtime = false;
    params.print_progress = false;
    params.print_special  = false;
    params.translate      = false;

    if (whisper_full(model_->ctx, params, samples, static_cast<int>(num_samples)) != 0)
        return {"", false};

    std::string text;
    int n = whisper_full_n_segments(model_->ctx);
    for (int i = 0; i < n; ++i)
        text += whisper_full_get_segment_text(model_->ctx, i);

    return {text, true};
}

void SttWhisper::shutdown() {
    if (model_) {
        WhisperModel::release(cfg_.model_path);
        model_ = nullptr;
    }
}
