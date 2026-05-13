#pragma once
#include "stt-backend.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

struct whisper_context;

// Shared whisper model — reference counted across Pipeline instances.
struct WhisperModel {
    whisper_context* ctx = nullptr;
    std::mutex mutex;  // whisper_full is not thread-safe
    int ref_count = 0;

    static WhisperModel& get(const std::string& model_path);
    static void release(const std::string& model_path);

private:
    static std::mutex registry_mutex_;
    static std::map<std::string, WhisperModel*> registry_;
};

class SttWhisper : public ISttBackend {
public:
    struct Config {
        std::string model_path;
        std::string language = "ja";
        int n_threads = 4;
    };

    explicit SttWhisper(Config cfg);
    ~SttWhisper() override;

    SttResult transcribe(const float* samples, size_t num_samples) override;
    void shutdown() override;

private:
    Config cfg_;
    WhisperModel* model_ = nullptr;
};
