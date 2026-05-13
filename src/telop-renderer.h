#pragma once
#include <obs-module.h>
#include <graphics/graphics.h>
#include <string>
#include <vector>
#include <cstdint>

struct TelopStyle {
    std::string font_family = "Hiragino Sans";
    float font_size = 48.f;
    bool outline = true;
    float outline_thickness = 3.f;
    uint32_t outline_color = 0xFF000000; // ARGB
    bool shadow = true;
    float shadow_offset_x = 2.f;
    float shadow_offset_y = 2.f;
    bool per_char_color = false;
    std::vector<uint32_t> palette = {0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF0000FF};
};

class TelopRenderer {
public:
    explicit TelopRenderer(const TelopStyle& style);
    ~TelopRenderer();

    // Re-render texture if text changed. Must be called on the OBS graphics thread.
    void update_text(const std::string& text);

    // Draw the texture at (x, y) with given alpha. Must be called on the OBS graphics thread.
    void render(float x, float y, float alpha);

    uint32_t texture_width()  const { return tex_w_; }
    uint32_t texture_height() const { return tex_h_; }

private:
    void rebuild_texture();

    TelopStyle style_;
    std::string current_text_;
    gs_texture_t* texture_ = nullptr;
    uint32_t tex_w_ = 0, tex_h_ = 0;
};
