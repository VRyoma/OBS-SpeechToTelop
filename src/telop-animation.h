#pragma once
#include <string>

enum class AnimState { Idle, Enter, Display, Exit };

class TelopAnimation {
public:
    TelopAnimation(float enter_s, float exit_s, float display_s);

    void trigger(const std::string& text);
    void tick(float dt);

    AnimState state() const { return state_; }
    float alpha() const;                  // 0..1 opacity for whole telop
    float char_delay(size_t char_idx) const; // stagger offset in seconds per character

private:
    float enter_s_, exit_s_, display_s_;
    AnimState state_ = AnimState::Idle;
    float elapsed_ = 0.f;
    size_t char_count_ = 0;

    static constexpr float kCharStagger = 0.04f; // seconds between each char pop-in
};
