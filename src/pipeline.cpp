#include "pipeline.h"

Pipeline::Pipeline()
    : pcm_ring(std::make_unique<RingBuffer<float, kPcmRingSize>>()) {}
