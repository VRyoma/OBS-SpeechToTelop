#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include "pipeline.h"
#include "stt-backend.h"
#include "text-buffer.h"

class SttEngine {
public:
    struct Config {
        int chunk_ms   = 3000;            // chunk length for inference
        float silence_threshold = 0.01f;  // RMS threshold below which we skip inference
        int text_timeout_ms = 5000;       // TextBuffer flush timeout
        bool instant_mode = false;        // false = sentence-boundary mode
    };

    SttEngine(Pipeline* pipeline, std::unique_ptr<ISttBackend> backend, Config cfg);
    ~SttEngine();

    void start();
    void stop();

private:
    void run();
    bool is_silence(const float* samples, size_t n) const;

    Pipeline* pipeline_;
    std::unique_ptr<ISttBackend> backend_;
    Config cfg_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    TextBuffer text_buf_;
};
