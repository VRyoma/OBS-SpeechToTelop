#include "telop-animation.h"
#include <algorithm>
#include <cmath>

TelopAnimation::TelopAnimation(float enter_s, float exit_s, float display_s)
    : enter_s_(enter_s), exit_s_(exit_s), display_s_(display_s) {}

void TelopAnimation::trigger(const std::string& text) {
    state_ = AnimState::Enter;
    elapsed_ = 0.f;
    // Count UTF-8 characters (not bytes)
    char_count_ = 0;
    for (const char* p = text.c_str(); *p; ) {
        unsigned char c = static_cast<unsigned char>(*p);
        if      (c < 0x80) p += 1;
        else if (c < 0xE0) p += 2;
        else if (c < 0xF0) p += 3;
        else               p += 4;
        ++char_count_;
    }
}

void TelopAnimation::tick(float dt) {
    elapsed_ += dt;
    switch (state_) {
    case AnimState::Enter:
        if (elapsed_ >= enter_s_) {
            elapsed_ = 0.f;
            state_ = AnimState::Display;
        }
        break;
    case AnimState::Display:
        if (elapsed_ >= display_s_) {
            elapsed_ = 0.f;
            state_ = AnimState::Exit;
        }
        break;
    case AnimState::Exit:
        if (elapsed_ >= exit_s_) {
            elapsed_ = 0.f;
            state_ = AnimState::Idle;
        }
        break;
    case AnimState::Idle:
        break;
    }
}

float TelopAnimation::alpha() const {
    switch (state_) {
    case AnimState::Idle:    return 0.f;
    case AnimState::Enter:   return enter_s_ > 0 ? std::min(elapsed_ / enter_s_, 1.f) : 1.f;
    case AnimState::Display: return 1.f;
    case AnimState::Exit:    return exit_s_ > 0 ? std::max(1.f - elapsed_ / exit_s_, 0.f) : 0.f;
    }
    return 0.f;
}

float TelopAnimation::char_delay(size_t idx) const {
    return static_cast<float>(idx) * kCharStagger;
}
