#pragma once
#include "stt-backend.h"
#include <string>
#include <vector>

class SttCloud : public ISttBackend {
public:
    struct Config {
        std::string api_key;
        std::string language = "ja";
        int max_retries = 3;
        long timeout_s = 30;
    };

    explicit SttCloud(Config cfg);
    SttResult transcribe(const float* samples, size_t num_samples) override;

private:
    std::string post_to_api(const float* samples, size_t num_samples);
    std::string parse_text_from_json(const std::string& json);
    static std::vector<uint8_t> encode_wav(const float* samples, size_t n);

    Config cfg_;
};
