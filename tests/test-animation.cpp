#include <gtest/gtest.h>
#include "telop-animation.h"

TEST(TelopAnimation, InitialStateIsIdle) {
    TelopAnimation anim(/*enter_s=*/0.5f, /*exit_s=*/0.5f, /*display_s=*/3.0f);
    EXPECT_EQ(anim.state(), AnimState::Idle);
    EXPECT_FLOAT_EQ(anim.alpha(), 0.f);
}

TEST(TelopAnimation, TriggerBeginsEnter) {
    TelopAnimation anim(0.5f, 0.5f, 3.0f);
    anim.trigger("Hello");
    EXPECT_EQ(anim.state(), AnimState::Enter);
}

TEST(TelopAnimation, TickThroughEnterToDisplay) {
    TelopAnimation anim(0.5f, 0.5f, 3.0f);
    anim.trigger("Hello");
    anim.tick(0.25f);
    EXPECT_EQ(anim.state(), AnimState::Enter);
    EXPECT_NEAR(anim.alpha(), 0.5f, 0.01f);
    anim.tick(0.3f);  // total 0.55s > 0.5s enter
    EXPECT_EQ(anim.state(), AnimState::Display);
    EXPECT_FLOAT_EQ(anim.alpha(), 1.f);
}

TEST(TelopAnimation, DisplayThenExit) {
    TelopAnimation anim(0.0f, 0.5f, 1.0f);
    anim.trigger("Hello");
    anim.tick(0.0f);  // enter=0 -> immediately Display
    EXPECT_EQ(anim.state(), AnimState::Display);
    anim.tick(1.1f);  // past display duration
    EXPECT_EQ(anim.state(), AnimState::Exit);
    anim.tick(0.6f);  // past exit duration
    EXPECT_EQ(anim.state(), AnimState::Idle);
    EXPECT_FLOAT_EQ(anim.alpha(), 0.f);
}

TEST(TelopAnimation, PerCharDelayIncreases) {
    TelopAnimation anim(1.0f, 0.5f, 3.0f);
    anim.trigger("ABC");
    float d0 = anim.char_delay(0);
    float d1 = anim.char_delay(1);
    float d2 = anim.char_delay(2);
    EXPECT_LT(d0, d1);
    EXPECT_LT(d1, d2);
}
