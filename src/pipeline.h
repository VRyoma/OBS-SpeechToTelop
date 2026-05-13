#pragma once
#include <mutex>
#include <string>
#include "ring-buffer.h"

// ~10 seconds of 16kHz audio = 160000 samples.
// Ring buffer holds 2x that for headroom.
constexpr size_t kPcmRingSize = 320001;  // N-1 usable = 320000

struct Pipeline {
    RingBuffer<float, kPcmRingSize> pcm_ring;

    std::mutex text_mutex;
    std::string latest_text;  // updated by STT thread
    bool text_updated = false;
};
