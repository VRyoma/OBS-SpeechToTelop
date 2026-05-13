#include <gtest/gtest.h>
#include "ring-buffer.h"
#include <thread>
#include <vector>

TEST(RingBuffer, PushAndPop) {
    RingBuffer<float, 8> rb;
    EXPECT_TRUE(rb.push(1.0f));
    float v = 0;
    EXPECT_TRUE(rb.pop(v));
    EXPECT_FLOAT_EQ(v, 1.0f);
}

TEST(RingBuffer, EmptyPopReturnsFalse) {
    RingBuffer<float, 8> rb;
    float v;
    EXPECT_FALSE(rb.pop(v));
}

TEST(RingBuffer, FullPushReturnsFalse) {
    RingBuffer<float, 4> rb;  // capacity is N-1 = 3
    EXPECT_TRUE(rb.push(1.f));
    EXPECT_TRUE(rb.push(2.f));
    EXPECT_TRUE(rb.push(3.f));
    EXPECT_FALSE(rb.push(4.f));  // full
}

TEST(RingBuffer, WrapAround) {
    RingBuffer<float, 4> rb;
    rb.push(1.f); rb.push(2.f); rb.push(3.f);
    float v;
    rb.pop(v); rb.pop(v);  // consume 1, 2
    rb.push(4.f); rb.push(5.f);  // wrap
    rb.pop(v); EXPECT_FLOAT_EQ(v, 3.f);
    rb.pop(v); EXPECT_FLOAT_EQ(v, 4.f);
    rb.pop(v); EXPECT_FLOAT_EQ(v, 5.f);
}

TEST(RingBuffer, ConcurrentSPSC) {
    RingBuffer<float, 1024> rb;
    constexpr int N = 10000;
    std::vector<float> received;
    received.reserve(N);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!rb.push(static_cast<float>(i))) {}
        }
    });
    std::thread consumer([&] {
        float v;
        for (int i = 0; i < N; ++i) {
            while (!rb.pop(v)) {}
            received.push_back(v);
        }
    });
    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(received[i], static_cast<float>(i));
}
