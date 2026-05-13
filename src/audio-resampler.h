#pragma once
#include <vector>
#include <cstdint>

// Converts float32 interleaved stereo or mono at `src_rate` Hz
// to mono float32 at 16000 Hz using linear interpolation.
class AudioResampler {
public:
    explicit AudioResampler(uint32_t src_rate, uint32_t channels);

    // Push `frames` frames of interleaved audio. Returns resampled mono samples.
    std::vector<float> process(const float* input, size_t frames);

private:
    uint32_t src_rate_;
    uint32_t channels_;
    double ratio_;       // dst_rate / src_rate = 16000 / src_rate
    double phase_ = 0.0; // fractional sample position
    float last_sample_ = 0.f;
};
