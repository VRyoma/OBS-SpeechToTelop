#include "telop-renderer.h"
#include <graphics/graphics.h>
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

    // Build attributed string
    CFMutableAttributedStringRef attr = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
    CFAttributedStringReplaceString(attr, CFRangeMake(0, 0), cf_text);
    CFRange full = CFRangeMake(0, CFStringGetLength(cf_text));
    CFAttributedStringSetAttribute(attr, full, kCTFontAttributeName, font);

    CTLineRef line = CTLineCreateWithAttributedString(attr);
    CGRect bounds = CTLineGetImageBounds(line, nullptr);

    float pad = style.outline_thickness * 2.f + 4.f;
    if (style.shadow) {
        pad += std::max(std::abs(style.shadow_offset_x), std::abs(style.shadow_offset_y)) + 2.f;
    }

    out_w = static_cast<uint32_t>(std::ceil(bounds.size.width  + pad * 2.f));
    out_h = static_cast<uint32_t>(std::ceil(bounds.size.height + pad * 2.f));
    if (out_w == 0 || out_h == 0) {
        CFRelease(line); CFRelease(attr); CFRelease(font); CFRelease(cf_text);
        return {};
    }

    std::vector<uint8_t> pixels(out_w * out_h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), out_w, out_h, 8,
        out_w * 4, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);

    float bx = pad - static_cast<float>(bounds.origin.x);
    float by = pad - static_cast<float>(bounds.origin.y);

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

    // Shadow pass
    if (style.shadow) {
        CGContextSaveGState(ctx);
        CGContextSetTextPosition(ctx, bx + style.shadow_offset_x, by - style.shadow_offset_y);
        CGColorRef sc = argb_color(0xAA000000);
        CGContextSetFillColorWithColor(ctx, sc);
        CGContextSetTextDrawingMode(ctx, kCGTextFill);
        CTLineDraw(line, ctx);
        CGColorRelease(sc);
        CGContextRestoreGState(ctx);
    }

    // Outline pass
    if (style.outline) {
        CGContextSaveGState(ctx);
        CGContextSetTextPosition(ctx, bx, by);
        CGColorRef oc = argb_color(style.outline_color);
        CGContextSetFillColorWithColor(ctx, oc);
        CGContextSetStrokeColorWithColor(ctx, oc);
        CGContextSetLineWidth(ctx, style.outline_thickness * 2.f);
        CGContextSetTextDrawingMode(ctx, kCGTextFillStroke);
        CTLineDraw(line, ctx);
        CGColorRelease(oc);
        CGContextRestoreGState(ctx);
    }

    // Body pass
    CGContextSaveGState(ctx);
    CGContextSetTextPosition(ctx, bx, by);
    CGContextSetTextDrawingMode(ctx, kCGTextFill);
    if (!style.per_char_color) {
        CGColorRef wc = argb_color(0xFFFFFFFF);
        CGContextSetFillColorWithColor(ctx, wc);
        CTLineDraw(line, ctx);
        CGColorRelease(wc);
    } else {
        CFIndex n_chars = CFStringGetLength(cf_text);
        for (CFIndex i = 0; i < n_chars; ++i) {
            uint32_t col = style.palette[i % style.palette.size()];
            CGFloat x_off = CTLineGetOffsetForStringIndex(line, i, nullptr);
            CFStringRef char_str = CFStringCreateWithSubstring(
                kCFAllocatorDefault, cf_text, CFRangeMake(i, 1));
            CFMutableAttributedStringRef char_attr = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
            CFAttributedStringReplaceString(char_attr, CFRangeMake(0, 0), char_str);
            CFAttributedStringSetAttribute(char_attr, CFRangeMake(0, 1), kCTFontAttributeName, font);
            CTLineRef char_line = CTLineCreateWithAttributedString(char_attr);
            CGContextSetTextPosition(ctx, bx + x_off, by);
            CGColorRef cc = argb_color(col);
            CGContextSetFillColorWithColor(ctx, cc);
            CTLineDraw(char_line, ctx);
            CGColorRelease(cc);
            CFRelease(char_line);
            CFRelease(char_attr);
            CFRelease(char_str);
        }
    }
    CGContextRestoreGState(ctx);

    CGContextRelease(ctx);
    CFRelease(line);
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
    Gdiplus::SolidBrush body(Gdiplus::Color(255, 255, 255, 255));
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
    auto pixels = render_text_to_rgba(current_text_, style_, w, h);
    if (w == 0 || h == 0 || pixels.empty()) {
        if (texture_) { gs_texture_destroy(texture_); texture_ = nullptr; }
        tex_w_ = tex_h_ = 0;
        return;
    }

    if (texture_) gs_texture_destroy(texture_);
    texture_ = gs_texture_create(w, h, GS_RGBA, 1, nullptr, GS_DYNAMIC);
    if (texture_) {
        gs_texture_set_image(texture_, pixels.data(), w * 4, false);
        tex_w_ = w;
        tex_h_ = h;
    }
}

void TelopRenderer::render(float x, float y, float alpha) {
    if (!texture_ || alpha <= 0.f) return;

    gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), texture_);

    gs_matrix_push();
    gs_matrix_translate3f(x, y, 0.f);

    while (gs_effect_loop(effect, "Draw")) {
        gs_draw_sprite(texture_, 0, tex_w_, tex_h_);
    }

    gs_matrix_pop();
}
