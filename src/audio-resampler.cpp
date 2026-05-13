#include "audio-resampler.h"
#include <cmath>

static constexpr uint32_t DST_RATE = 16000;

AudioResampler::AudioResampler(uint32_t src_rate, uint32_t channels)
    : src_rate_(src_rate), channels_(channels),
      ratio_(static_cast<double>(DST_RATE) / src_rate) {}

std::vector<float> AudioResampler::process(const float* input, size_t frames)
{
    // Mix channels to mono first
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        float s = 0.f;
        for (uint32_t c = 0; c < channels_; ++c)
            s += input[i * channels_ + c];
        mono[i] = s / channels_;
    }

    // Linear interpolation resample
    size_t dst_frames = static_cast<size_t>(std::ceil(frames * ratio_));
    std::vector<float> out;
    out.reserve(dst_frames);

    for (size_t di = 0; ; ++di) {
        double src_pos = di / ratio_ + (1.0 - phase_);
        size_t si = static_cast<size_t>(src_pos);
        if (si >= frames) {
            phase_ = src_pos - frames;
            last_sample_ = mono.back();
            break;
        }
        double frac = src_pos - si;
        float s0 = (si == 0) ? last_sample_ : mono[si - 1];
        float s1 = mono[si];
        out.push_back(static_cast<float>(s0 + frac * (s1 - s0)));
    }
    return out;
}
