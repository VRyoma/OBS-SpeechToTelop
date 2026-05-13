#include <gtest/gtest.h>
#include "text-buffer.h"
#include <chrono>
#include <thread>

TEST(TextBuffer, EmitOnSentenceBoundary) {
    TextBuffer buf(/*timeout_ms=*/5000);
    buf.push("こんにちは");
    EXPECT_EQ(buf.try_pop(), "");  // no boundary yet
    buf.push("。");
    EXPECT_EQ(buf.try_pop(), "こんにちは。");
}

TEST(TextBuffer, MultipleFragmentsBeforeBoundary) {
    TextBuffer buf(5000);
    buf.push("今日は");
    buf.push("いい天気");
    buf.push("です");
    EXPECT_EQ(buf.try_pop(), "");
    buf.push("！");
    EXPECT_EQ(buf.try_pop(), "今日はいい天気です！");
}

TEST(TextBuffer, AsciiSentenceBoundary) {
    TextBuffer buf(5000);
    buf.push("Hello");
    buf.push(".");
    EXPECT_EQ(buf.try_pop(), "Hello.");
}

TEST(TextBuffer, TimeoutFlush) {
    TextBuffer buf(/*timeout_ms=*/50);
    buf.push("未完の文");
    EXPECT_EQ(buf.try_pop(), "");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(buf.try_pop(), "未完の文");
}

TEST(TextBuffer, ClearResetsBuffer) {
    TextBuffer buf(5000);
    buf.push("text");
    buf.clear();
    buf.push("new。");
    EXPECT_EQ(buf.try_pop(), "new。");
}
