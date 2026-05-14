#include "stt-engine.h"
#include <obs-module.h>
#include <cmath>
#include <chrono>

SttEngine::SttEngine(Pipeline* pipeline, BackendFactory factory, Config cfg)
    : pipeline_(pipeline), factory_(std::move(factory)), cfg_(cfg),
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

void SttEngine::set_status(const std::string& msg) {
    std::lock_guard<std::mutex> lock(pipeline_->text_mutex);
    pipeline_->latest_text = msg;
    pipeline_->text_updated = true;
}

void SttEngine::run() {
    blog(LOG_INFO, "[SpeechToTelop] engine thread started");
    try {
        set_status("初期化中...");
        blog(LOG_INFO, "[SpeechToTelop] loading backend...");
        backend_ = factory_();
        blog(LOG_INFO, "[SpeechToTelop] backend ready, starting transcription");
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "[SpeechToTelop] backend init failed: %s", e.what());
        set_status(std::string("エラー: ") + e.what());
        return;
    } catch (...) {
        blog(LOG_ERROR, "[SpeechToTelop] backend init failed (unknown)");
        set_status("エラー: バックエンドの初期化に失敗しました");
        return;
    }

    // Whisper needs at least 3 seconds for reliable output; clamp cfg_ to a minimum.
    const int effective_chunk_ms = std::max(cfg_.chunk_ms, 3000);
    const size_t chunk_samples = static_cast<size_t>(16000.0 * effective_chunk_ms / 1000.0);
    std::vector<float> accum;
    accum.reserve(chunk_samples * 2);

    while (running_) {
        // Drain ring buffer
        float s;
        while (pipeline_->pcm_ring->pop(s))
            accum.push_back(s);

        if (accum.size() >= chunk_samples) {
            float rms = 0.f;
            for (size_t i = 0; i < chunk_samples; ++i) rms += accum[i] * accum[i];
            rms = std::sqrt(rms / static_cast<float>(chunk_samples));
            blog(LOG_INFO, "[SpeechToTelop] chunk rms=%.4f threshold=%.4f silence=%s",
                 rms, cfg_.silence_threshold, rms < cfg_.silence_threshold ? "YES" : "NO");
            if (rms >= cfg_.silence_threshold) {
                SttResult result = backend_->transcribe(accum.data(), chunk_samples);
                blog(LOG_INFO, "[SpeechToTelop] result: '%s'", result.text.c_str());
                if (!result.text.empty()) {
                    std::lock_guard<std::mutex> lock(pipeline_->text_mutex);
                    pipeline_->latest_text = result.text;
                    pipeline_->text_updated = true;
                }
            }
            // Shift consumed samples out
            accum.erase(accum.begin(), accum.begin() + chunk_samples);
        } else {
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
