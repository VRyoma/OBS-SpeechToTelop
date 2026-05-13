#include <gtest/gtest.h>
#include <algorithm>
#include "pipeline-registry.h"
#include "pipeline.h"

TEST(PipelineRegistry, RegisterAndFind) {
    PipelineRegistry reg;
    Pipeline p;
    reg.register_pipeline("mic-0", &p);
    EXPECT_EQ(reg.find("mic-0"), &p);
}

TEST(PipelineRegistry, UnregisterRemoves) {
    PipelineRegistry reg;
    Pipeline p;
    reg.register_pipeline("mic-0", &p);
    reg.unregister_pipeline("mic-0");
    EXPECT_EQ(reg.find("mic-0"), nullptr);
}

TEST(PipelineRegistry, ListReturnsAllIds) {
    PipelineRegistry reg;
    Pipeline p1, p2;
    reg.register_pipeline("a", &p1);
    reg.register_pipeline("b", &p2);
    auto ids = reg.list_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_NE(std::find(ids.begin(), ids.end(), "a"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "b"), ids.end());
}

TEST(PipelineRegistry, DuplicateIdOverwrites) {
    PipelineRegistry reg;
    Pipeline p1, p2;
    reg.register_pipeline("x", &p1);
    reg.register_pipeline("x", &p2);
    EXPECT_EQ(reg.find("x"), &p2);
}

TEST(PipelineRegistry, GlobalSingleton) {
    Pipeline p;
    PipelineRegistry::global().register_pipeline("g-test", &p);
    EXPECT_EQ(PipelineRegistry::global().find("g-test"), &p);
    PipelineRegistry::global().unregister_pipeline("g-test");
}
