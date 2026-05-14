#include "telop-renderer.h"
#include <graphics/graphics.h>
#include <obs-module.h>
#include <vector>
#include <cstring>

// ─── Platform text rendering ──────────────────────────────────────────────
#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

static std::vector<uint8_t> render_text_to_rgba(
    const std::string& text, const TelopStyle& style,
    uint32_t& out_w, uint32_t& out_h)
{
    if (text.empty()) { out_w = out_h = 0; return {}; }

    CFStringRef cf_text = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.c_str()), text.size(),
        kCFStringEncodingUTF8, false);
    if (!cf_text) { out_w = out_h = 0; return {}; }

    CFStringRef font_name_str = CFStringCreateWithCString(
        kCFAllocatorDefault, style.font_family.c_str(), kCFStringEncodingUTF8);
    CTFontRef font = CTFontCreateWithName(font_name_str, style.font_size, nullptr);
    CFRelease(font_name_str);

    auto argb_color = [](uint32_t argb) -> CGColorRef {
        float a = ((argb >> 24) & 0xFF) / 255.f;
        float r = ((argb >> 16) & 0xFF) / 255.f;
        float g = ((argb >>  8) & 0xFF) / 255.f;
        float b = ((argb >>  0) & 0xFF) / 255.f;
        CGFloat comps[4] = {r, g, b, a};
        CGColorSpaceRef srgb = CGColorSpaceCreateDeviceRGB();
        CGColorRef c = CGColorCreate(srgb, comps);
        CGColorSpaceRelease(srgb);
        return c;
    };

    // Build attributed string — set foreground color so CTLineDraw uses the right color
    CFMutableAttributedStringRef attr = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
    CFAttributedStringReplaceString(attr, CFRangeMake(0, 0), cf_text);
    CFRange full = CFRangeMake(0, CFStringGetLength(cf_text));
    CFAttributedStringSetAttribute(attr, full, kCTFontAttributeName, font);

    // Measure bounds using a temporary line (no color needed for measurement)
    CTLineRef measure_line = CTLineCreateWithAttributedString(attr);
    CGRect bounds = CTLineGetImageBounds(measure_line, nullptr);
    CFRelease(measure_line);

    float pad = style.outline_thickness * 2.f + 4.f;
    if (style.shadow) {
        pad += std::max(std::abs(style.shadow_offset_x), std::abs(style.shadow_offset_y)) + 2.f;
    }

    out_w = static_cast<uint32_t>(std::ceil(bounds.size.width  + pad * 2.f));
    out_h = static_cast<uint32_t>(std::ceil(bounds.size.height + pad * 2.f));
    if (out_w == 0 || out_h == 0) {
        CFRelease(attr); CFRelease(font); CFRelease(cf_text);
        return {};
    }

    std::vector<uint8_t> pixels(out_w * out_h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), out_w, out_h, 8,
        out_w * 4, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);

    float bx = pad - static_cast<float>(bounds.origin.x);
    float by = pad - static_cast<float>(bounds.origin.y);

    // Helper: create a CTLine with a specific foreground color set via attributed string
    auto make_line_with_color = [&](uint32_t argb_col) -> CTLineRef {
        CGColorRef c = argb_color(argb_col);
        CFAttributedStringSetAttribute(attr, full, kCTForegroundColorAttributeName, c);
        CGColorRelease(c);
        return CTLineCreateWithAttributedString(attr);
    };

    // Background (座布団) pass
    if (style.background) {
        CGColorRef bgc = argb_color(style.background_color);
        CGContextSetFillColorWithColor(ctx, bgc);
        CGContextFillRect(ctx, CGRectMake(0, 0, out_w, out_h));
        CGColorRelease(bgc);
    }

    // Shadow pass — draw with semi-transparent black foreground color
    if (style.shadow) {
        CTLineRef shadow_line = make_line_with_color(0xAA000000);
        CGContextSaveGState(ctx);
        CGContextSetTextPosition(ctx, bx + style.shadow_offset_x, by - style.shadow_offset_y);
        CGContextSetTextDrawingMode(ctx, kCGTextFill);
        CTLineDraw(shadow_line, ctx);
        CGContextRestoreGState(ctx);
        CFRelease(shadow_line);
    }

    // Outline pass — use stroke mode; stroke color set via CGContext
    if (style.outline) {
        CTLineRef outline_line = make_line_with_color(style.outline_color);
        CGColorRef oc = argb_color(style.outline_color);
        CGContextSaveGState(ctx);
        CGContextSetTextPosition(ctx, bx, by);
        CGContextSetStrokeColorWithColor(ctx, oc);
        CGContextSetLineWidth(ctx, style.outline_thickness * 2.f);
        CGContextSetLineJoin(ctx, kCGLineJoinRound);
        CGContextSetLineCap(ctx, kCGLineCapRound);
        CGContextSetTextDrawingMode(ctx, kCGTextStroke);
        CTLineDraw(outline_line, ctx);
        CGColorRelease(oc);
        CGContextRestoreGState(ctx);
        CFRelease(outline_line);
    }

    // Body pass — foreground color set via kCTForegroundColorAttributeName
    {
        CTLineRef body_line = make_line_with_color(style.text_color);
        CGContextSaveGState(ctx);
        CGContextSetTextPosition(ctx, bx, by);
        CGContextSetTextDrawingMode(ctx, kCGTextFill);
        CTLineDraw(body_line, ctx);
        CGContextRestoreGState(ctx);
        CFRelease(body_line);
    }

    CGContextRelease(ctx);
    CFRelease(attr);
    CFRelease(font);
    CFRelease(cf_text);

    return pixels;
}

#elif defined(_WIN32)
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static std::vector<uint8_t> render_text_to_rgba(
    const std::string& text, const TelopStyle& style,
    uint32_t& out_w, uint32_t& out_h)
{
    if (text.empty()) { out_w = out_h = 0; return {}; }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wtext(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);

    int wflen = MultiByteToWideChar(CP_UTF8, 0, style.font_family.c_str(), -1, nullptr, 0);
    std::wstring wfont(wflen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, style.font_family.c_str(), -1, wfont.data(), wflen);

    Gdiplus::GdiplusStartupInput gdi_in;
    ULONG_PTR gdi_tok;
    Gdiplus::GdiplusStartup(&gdi_tok, &gdi_in, nullptr);

    Gdiplus::Font font(wfont.c_str(), style.font_size, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    {
        Gdiplus::Bitmap mb(1, 1, PixelFormat32bppARGB);
        Gdiplus::Graphics* mg = Gdiplus::Graphics::FromImage(&mb);
        Gdiplus::RectF bounds;
        Gdiplus::PointF origin(0, 0);
        mg->MeasureString(wtext.c_str(), -1, &font, origin, &bounds);
        delete mg;

        float pad = style.outline_thickness * 2.f + 4.f;
        out_w = static_cast<uint32_t>(bounds.Width  + pad * 2.f);
        out_h = static_cast<uint32_t>(bounds.Height + pad * 2.f);
    }
    if (out_w == 0 || out_h == 0) { Gdiplus::GdiplusShutdown(gdi_tok); return {}; }

    Gdiplus::Bitmap bmp(out_w, out_h, PixelFormat32bppARGB);
    Gdiplus::Graphics* g = Gdiplus::Graphics::FromImage(&bmp);
    if (style.background)
        g->Clear(argb_gdip(style.background_color));
    else
        g->Clear(Gdiplus::Color(0, 0, 0, 0));
    g->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    float pad = style.outline_thickness * 2.f + 4.f;
    Gdiplus::RectF rect(pad, pad,
        static_cast<float>(out_w) - pad * 2.f,
        static_cast<float>(out_h) - pad * 2.f);

    auto argb_gdip = [](uint32_t argb) {
        return Gdiplus::Color(
            (argb >> 24) & 0xFF, (argb >> 16) & 0xFF,
            (argb >>  8) & 0xFF,  argb        & 0xFF);
    };

    if (style.shadow) {
        Gdiplus::SolidBrush sb(Gdiplus::Color(0xAA, 0, 0, 0));
        Gdiplus::RectF sr(rect.X + style.shadow_offset_x, rect.Y + style.shadow_offset_y,
                          rect.Width, rect.Height);
        g->DrawString(wtext.c_str(), -1, &font, sr, nullptr, &sb);
    }
    if (style.outline) {
        Gdiplus::SolidBrush ob(argb_gdip(style.outline_color));
        float t = style.outline_thickness;
        for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            Gdiplus::RectF or_(rect.X + dx * t, rect.Y + dy * t, rect.Width, rect.Height);
            g->DrawString(wtext.c_str(), -1, &font, or_, nullptr, &ob);
        }
    }
    Gdiplus::SolidBrush body(argb_gdip(style.text_color));
    g->DrawString(wtext.c_str(), -1, &font, rect, nullptr, &body);
    delete g;

    Gdiplus::BitmapData bdata;
    Gdiplus::Rect lr(0, 0, out_w, out_h);
    bmp.LockBits(&lr, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bdata);
    std::vector<uint8_t> pixels(out_w * out_h * 4);
    for (uint32_t y = 0; y < out_h; ++y) {
        const uint8_t* src = static_cast<const uint8_t*>(bdata.Scan0) + y * bdata.Stride;
        uint8_t* dst = pixels.data() + y * out_w * 4;
        for (uint32_t x = 0; x < out_w; ++x) {
            // GDI+ BGRA -> RGBA
            dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
            src += 4; dst += 4;
        }
    }
    bmp.UnlockBits(&bdata);
    Gdiplus::GdiplusShutdown(gdi_tok);
    return pixels;
}
#else
// Stub for unsupported platforms
static std::vector<uint8_t> render_text_to_rgba(
    const std::string&, const TelopStyle&, uint32_t& out_w, uint32_t& out_h)
{
    out_w = out_h = 0;
    return {};
}
#endif // platform

// ─── TelopRenderer ───────────────────────────────────────────────────────
TelopRenderer::TelopRenderer(const TelopStyle& style) : style_(style) {}

TelopRenderer::~TelopRenderer() {
    if (texture_) {
        obs_enter_graphics();
        gs_texture_destroy(texture_);
        obs_leave_graphics();
    }
}

void TelopRenderer::update_text(const std::string& text) {
    if (text == current_text_) return;
    current_text_ = text;
    rebuild_texture();
}

void TelopRenderer::rebuild_texture() {
    uint32_t w = 0, h = 0;
    base_pixels_ = render_text_to_rgba(current_text_, style_, w, h);
    if (w == 0 || h == 0 || base_pixels_.empty()) {
        if (texture_) { gs_texture_destroy(texture_); texture_ = nullptr; }
        tex_w_ = tex_h_ = 0;
        base_pixels_.clear();
        return;
    }
    if (texture_) { gs_texture_destroy(texture_); texture_ = nullptr; }
    texture_ = gs_texture_create(w, h, GS_RGBA, 1, nullptr, GS_DYNAMIC);
    if (texture_) {
        tex_w_ = w;
        tex_h_ = h;
        uploaded_alpha_ = -1.f;  // force re-upload on next render call
    }
}

void TelopRenderer::upload_with_alpha(float alpha) {
    if (!texture_ || base_pixels_.empty()) return;
    if (std::abs(alpha - uploaded_alpha_) < 0.01f) return;
    uploaded_alpha_ = alpha;

    if (alpha >= 1.f) {
        gs_texture_set_image(texture_, base_pixels_.data(), tex_w_ * 4, false);
    } else {
        std::vector<uint8_t> mod(base_pixels_.size());
        for (size_t i = 0; i < base_pixels_.size(); ++i)
            mod[i] = static_cast<uint8_t>(base_pixels_[i] * alpha);
        gs_texture_set_image(texture_, mod.data(), tex_w_ * 4, false);
    }
}

void TelopRenderer::render(float x, float y, float alpha) {
    if (!texture_ || alpha <= 0.f) return;

    upload_with_alpha(alpha);

    gs_matrix_push();
    gs_matrix_translate3f(x, y, 0.f);

    gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
    while (gs_effect_loop(effect, "Draw")) {
        gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), texture_);
        gs_draw_sprite(texture_, 0, tex_w_, tex_h_);
    }

    gs_matrix_pop();
}
