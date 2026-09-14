#pragma once

#include "engine/platform/window/host.hpp"
#include "engine/render/passes/ui/backdrop.hpp"
#include "engine/ui/text/font.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace forge::ui {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
};

inline Color rgba(float r, float g, float b, float a = 1.0f) { return {r, g, b, a}; }

// Per-text effects in the UMG spirit, resolved against the SDF atlas.
// Outline and glow share the atlas padding, so very wide values clamp;
// zeroed effects cost nothing and keep solid quads untouched.
struct TextStyle {
    Color outline{0, 0, 0, 0};
    float outline_px = 0;
    float softness_px = 0;
    Color glow{0, 0, 0, 0};
    float glow_px = 0;
    float glow_strength = 1.0f;
    Color shadow{0, 0, 0, 0};
    float shadow_x = 0;
    float shadow_y = 0;
};

// Shader-ready SDF thresholds in normalized distance units (edge = 0.5).
struct TextFx {
    float outline_outer = 0.5f;
    float aa = 0.0f;
    float glow_outer = 0.5f;
    float glow_strength = 0.0f;
};

// Rounded-box style resolved to a signed-distance shape in the UI shader.
// Zero radius/border/shadow falls back to a plain quad automatically.
// backdrop_px blurs the scene behind the panel (frosted glass); the fill
// color tints the result. Shares the text-fx slot, so text is unaffected.
struct BoxStyle {
    Color fill{0, 0, 0, 0};
    Color border{0, 0, 0, 0};
    float radius = 0;
    float border_px = 0;
    float softness_px = 0;
    Color shadow{0, 0, 0, 0};
    float shadow_blur = 8.0f;
    float shadow_x = 0;
    float shadow_y = 0;
    float backdrop_px = 0;
};

// Clip marks split the batched vertex buffer into scissored runs at submit.
// Pure function of emission order, unit-tested without a GPU.
struct ClipMark {
    std::uint32_t start = 0;
    Rect rect{0, 0, 0, 0};
    bool enabled = false;
};

struct ClipRun {
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    Rect rect{0, 0, 0, 0};
    bool clipped = false;
};

inline std::vector<ClipRun> clip_runs(std::uint32_t total, const std::vector<ClipMark>& marks)
{
    std::vector<ClipRun> runs;
    std::uint32_t at = 0;
    bool enabled = false;
    Rect rect{0, 0, 0, 0};
    for (const auto& mark : marks) {
        const std::uint32_t end = std::min(mark.start, total);
        if (end > at) {
            runs.push_back(ClipRun{at, end, rect, enabled});
            at = end;
        }
        enabled = mark.enabled;
        rect = mark.rect;
    }
    if (at < total) runs.push_back(ClipRun{at, total, rect, enabled});
    return runs;
}

inline TextFx resolve_text_fx(const TextStyle& style, float scale, float sdf_texels_per_px, float sdf_units_per_texel)
{
    TextFx fx;
    if (scale <= 0.0f || sdf_texels_per_px <= 0.0f || sdf_units_per_texel <= 0.0f) return fx;
    const float units_per_px = sdf_texels_per_px / scale * sdf_units_per_texel;
    fx.aa = units_per_px * std::max(1.0f, style.softness_px);
    if (style.outline_px > 0.0f && style.outline.a > 0.0f)
        fx.outline_outer = std::max(0.0f, 0.5f - style.outline_px * units_per_px);
    const float extent = style.outline_px + style.glow_px;
    if (style.glow_strength > 0.0f && style.glow.a > 0.0f && extent > style.outline_px) {
        fx.glow_outer = std::max(0.0f, 0.5f - extent * units_per_px);
        fx.glow_strength = style.glow_strength;
    }
    return fx;
}

class Ui {
public:
    bool create(rhi::Host& host);
    void destroy(rhi::Host& host);
    void begin(int width, int height, float mouse_x, float mouse_y, bool mouse_down, bool pressed = false, bool released = false);
    void feed_text(std::string_view utf8);
    void key_backspace();
    bool wants_text() const { return field_active_ != 0; }

    void quad(Rect rect, Color color);
    void box(Rect rect, const BoxStyle& style);
    void text(float x, float y, std::string_view s, Color color, float scale = 1.0f);
    void text_center(float cx, float y, std::string_view s, Color color, float scale = 1.0f);
    void set_text_style(const TextStyle& style) { text_style_ = style; }
    void reset_text_style() { text_style_ = {}; }
    const TextStyle& text_style() const { return text_style_; }
    void set_time(float seconds) { time_ = seconds; }
    bool button(std::uint32_t id, Rect rect, std::string_view label);
    bool checkbox(std::uint32_t id, Rect rect, bool* value, std::string_view label);
    bool slider(std::uint32_t id, Rect rect, float* value, float min, float max, std::string_view label);
    bool field(std::uint32_t id, Rect rect, char* buffer, int capacity);
    bool cycle(std::uint32_t id, Rect rect, int* index, const char* const* items, int count);
    bool tabs(std::uint32_t id, Rect rect, const char* const* items, int count, int* selected);
    bool dropdown(std::uint32_t id, Rect rect, const char* const* items, int count, int* index);
    void progress(Rect rect, float frac);
    void separator(float x0, float x1, float y);
    void clip_begin(Rect rect);
    void clip_end();
    bool scrollbox(std::uint32_t id, Rect rect, float content_h, float* scroll);

    bool submit(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, bool clear, SDL_FColor clear_color,
                std::uint32_t target_w, std::uint32_t target_h);
    bool wants_backdrop() const { return needs_backdrop_; }
    bool prepare_backdrop(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, std::uint32_t target_w,
                          std::uint32_t target_h);

    int width() const { return width_; }
    int height() const { return height_; }
    const Font& font() const { return font_; }

private:
    struct Vertex {
        float x, y, u, v, r, g, b, a;
        float or_, og, ob, oa;
        float gr, gg, gb, ga;
        float fx_outer, fx_aa, fx_glow, fx_gs;
        float sx, sy, sw, sh;
        float sr, sb, sm, ss;
    };

    void vertex(float x, float y, float u, float v, Color color);
    void glyph_vertex(float x, float y, float u, float v, Color color, const TextStyle& style, const TextFx& fx);
    void shape_vertex(float x, float y, Rect box, const BoxStyle& style, Color fill);
    void emit_shape(Rect quad_rect, Rect box, const BoxStyle& style, Color fill);
    void styled_box(Rect rect, Color fill);
    bool hit(Rect rect) const;
    std::uint32_t hot_ = 0;
    std::uint32_t active_ = 0;
    std::uint32_t field_active_ = 0;
    bool mouse_down_ = false;
    bool mouse_pressed_ = false;
    bool mouse_released_ = false;
    bool was_down_ = false;
    float mx_ = 0, my_ = 0;
    int width_ = 1, height_ = 1;
    TextStyle text_style_{};
    float time_ = 0;
    std::uint32_t hover_id_ = 0;
    float hover_since_ = 0;
    std::uint32_t drop_open_ = 0;
    std::vector<ClipMark> clips_;
    Backdrop backdrop_;
    bool needs_backdrop_ = false;
    Color hover_fill(std::uint32_t id, bool over, bool down);
    std::string typed_;
    bool backspace_ = false;
    Font font_;
    std::vector<Vertex> verts_;
    SDL_GPUTexture* atlas_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    SDL_GPUBuffer* vertices_ = nullptr;
    SDL_GPUTransferBuffer* transfer_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    Uint32 vertex_capacity_ = 0;
};

inline std::uint32_t hash_id(std::string_view s)
{
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

// Immediate-mode layout cursors: pure rect math, no GPU state.
struct VCursor {
    Rect area{};
    float y = 0;
    float gap = 8;
    explicit VCursor(Rect a, float g = 8) : area(a), y(a.y), gap(g) {}
    Rect next(float h)
    {
        const Rect out{area.x, y, area.w, h};
        y += h + gap;
        return out;
    }
    float remaining() const { return area.y + area.h - y; }
};

struct HCursor {
    Rect area{};
    float x = 0;
    float gap = 8;
    explicit HCursor(Rect a, float g = 8) : area(a), x(a.x), gap(g) {}
    Rect next(float w)
    {
        const Rect out{x, area.y, w, area.h};
        x += w + gap;
        return out;
    }
    float remaining() const { return area.x + area.w - x; }
};

} // namespace forge::ui
