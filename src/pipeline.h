#pragma once
#include <memory>
#include <mutex>
#include <string>
#include "ring-buffer.h"

// ~10 seconds of 16kHz audio = 160000 samples.
// Ring buffer holds 2x that for headroom.
constexpr size_t kPcmRingSize = 320001;  // N-1 usable = 320000

struct Pipeline {
    // Heap-allocated: 320001 * 4 = ~1.28 MB would overflow the Windows
    // default 1 MB stack when Pipeline is declared as a local variable.
    std::unique_ptr<RingBuffer<float, kPcmRingSize>> pcm_ring;

    std::mutex text_mutex;
    std::string latest_text;
    bool text_updated = false;

    Pipeline();
};
