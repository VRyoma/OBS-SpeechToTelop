#pragma once
#include <string>

struct SttResult {
    std::string text;
    bool is_final;  // false = partial, true = sentence complete
};

class ISttBackend {
public:
    virtual ~ISttBackend() = default;

    // Transcribe `num_samples` float32 mono 16kHz samples. Blocking call.
    virtual SttResult transcribe(const float* samples, size_t num_samples) = 0;

    virtual void shutdown() {}
};
