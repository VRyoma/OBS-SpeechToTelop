#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include "pipeline.h"
#include "stt-backend.h"
#include "text-buffer.h"

class SttEngine {
public:
    struct Config {
        int chunk_ms   = 3000;
        float silence_threshold = 0.005f;
        int text_timeout_ms = 5000;
        bool instant_mode = false;
    };

    using BackendFactory = std::function<std::unique_ptr<ISttBackend>()>;

    SttEngine(Pipeline* pipeline, BackendFactory factory, Config cfg);
    ~SttEngine();

    void start();
    void stop();

private:
    void run();
    bool is_silence(const float* samples, size_t n) const;
    void set_status(const std::string& msg);

    Pipeline* pipeline_;
    BackendFactory factory_;
    std::unique_ptr<ISttBackend> backend_;
    Config cfg_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    TextBuffer text_buf_;
};
