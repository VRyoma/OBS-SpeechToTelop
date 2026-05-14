#include "stt-engine.h"
#include <cmath>
#include <chrono>

SttEngine::SttEngine(Pipeline* pipeline, std::unique_ptr<ISttBackend> backend, Config cfg)
    : pipeline_(pipeline), backend_(std::move(backend)), cfg_(cfg),
      text_buf_(cfg.text_timeout_ms) {}

SttEngine::~SttEngine() { stop(); }

void SttEngine::start() {
    running_ = true;
    thread_ = std::thread(&SttEngine::run, this);
}

void SttEngine::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void SttEngine::run() {
    const size_t chunk_samples = static_cast<size_t>(16000.0 * cfg_.chunk_ms / 1000.0);
    std::vector<float> accum;
    accum.reserve(chunk_samples * 2);

    while (running_) {
        // Drain ring buffer
        float s;
        while (pipeline_->pcm_ring->pop(s))
            accum.push_back(s);

        if (accum.size() >= chunk_samples) {
            if (!is_silence(accum.data(), chunk_samples)) {
                SttResult result = backend_->transcribe(accum.data(), chunk_samples);
                if (!result.text.empty()) {
                    text_buf_.push(result.text);
                    std::string sentence = text_buf_.try_pop();
                    if (!sentence.empty()) {
                        std::lock_guard<std::mutex> lock(pipeline_->text_mutex);
                        pipeline_->latest_text = sentence;
                        pipeline_->text_updated = true;
                    }
                }
            }
            // Shift consumed samples out
            accum.erase(accum.begin(), accum.begin() + chunk_samples);
        } else {
            // Check timeout flush even without a full chunk
            std::string sentence = text_buf_.try_pop();
            if (!sentence.empty()) {
                std::lock_guard<std::mutex> lock(pipeline_->text_mutex);
                pipeline_->latest_text = sentence;
                pipeline_->text_updated = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool SttEngine::is_silence(const float* samples, size_t n) const {
    float rms = 0.f;
    for (size_t i = 0; i < n; ++i) rms += samples[i] * samples[i];
    rms = std::sqrt(rms / static_cast<float>(n));
    return rms < cfg_.silence_threshold;
}
