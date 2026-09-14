#include "engine/ui/ui.hpp"

#include "engine/core/paths.hpp"
#include "engine/rhi/shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace forge::ui {
namespace {

constexpr Color kText{0.92f, 0.93f, 0.90f, 1.0f};
constexpr Color kMuted{0.62f, 0.64f, 0.60f, 1.0f};
constexpr Color kBtn{0.14f, 0.20f, 0.16f, 1.0f};
constexpr Color kHover{0.22f, 0.40f, 0.28f, 1.0f};
constexpr Color kDown{0.10f, 0.32f, 0.22f, 1.0f};

} // namespace

bool Ui::create(rhi::Host& host)
{
    destroy(host);
    const auto font_path = forge::assets_directory() / "fonts/DroidSans.ttf";
    if (!font_.bake(font_path, 28.0f)) {
        SDL_Log("UI font bake failed: %s", font_path.string().c_str());
        return false;
    }

    SDL_GPUTextureCreateInfo tex{};
    tex.type = SDL_GPU_TEXTURETYPE_2D;
    tex.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tex.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex.width = static_cast<Uint32>(font_.atlas_width());
    tex.height = static_cast<Uint32>(font_.atlas_height());
    tex.layer_count_or_depth = 1;
    tex.num_levels = 1;
    atlas_ = SDL_CreateGPUTexture(host.device(), &tex);
    if (!atlas_) return false;

    const auto bytes = static_cast<Uint32>(font_.atlas_width() * font_.atlas_height());
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bytes;
    auto* upload = SDL_CreateGPUTransferBuffer(host.device(), &tb);
    if (!upload) return false;
    auto* map = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(host.device(), upload, false));
    if (!map) {
        SDL_ReleaseGPUTransferBuffer(host.device(), upload);
        return false;
    }
    std::memcpy(map, font_.pixels(), bytes);
    SDL_UnmapGPUTransferBuffer(host.device(), upload);

    rhi::Command command(host.device());
    if (!command.handle) {
        SDL_ReleaseGPUTransferBuffer(host.device(), upload);
        return false;
    }
    auto* copy = SDL_BeginGPUCopyPass(command.handle);
    if (!copy) {
        SDL_ReleaseGPUTransferBuffer(host.device(), upload);
        return false;
    }
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = upload;
    SDL_GPUTextureRegion dst{};
    dst.texture = atlas_;
    dst.w = tex.width;
    dst.h = tex.height;
    dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = command.submit();
    SDL_ReleaseGPUTransferBuffer(host.device(), upload);
    if (!submitted || !SDL_WaitForGPUIdle(host.device())) return false;

    SDL_GPUSamplerCreateInfo samp{};
    samp.min_filter = SDL_GPU_FILTER_LINEAR;
    samp.mag_filter = SDL_GPU_FILTER_LINEAR;
    samp.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samp.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samp.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(host.device(), &samp);

    backdrop_.set_format(SDL_GetGPUSwapchainTextureFormat(host.device(), host.window()));
    if (!backdrop_.create(host.device())) return false;
    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"ui.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"ui.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        backdrop_.destroy(host.device());
        return false;
    }
    SDL_GPUVertexBufferDescription buffer{};
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
        {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 80},
        {7, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 96},
    };
    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GetGPUSwapchainTextureFormat(host.device(), host.window());
    color.blend_state.enable_blend = true;
    color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B
        | SDL_GPU_COLORCOMPONENT_A;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 8;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline_ && sampler_ && atlas_ && backdrop_.sampler() && backdrop_.texture();
}

void Ui::destroy(rhi::Host& host)
{
    if (!host.device()) return;
    backdrop_.destroy(host.device());
    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pipeline_);
    if (vertices_) SDL_ReleaseGPUBuffer(host.device(), vertices_);
    if (transfer_) SDL_ReleaseGPUTransferBuffer(host.device(), transfer_);
    if (sampler_) SDL_ReleaseGPUSampler(host.device(), sampler_);
    if (atlas_) SDL_ReleaseGPUTexture(host.device(), atlas_);
    pipeline_ = nullptr;
    vertices_ = nullptr;
    transfer_ = nullptr;
    sampler_ = nullptr;
    atlas_ = nullptr;
    vertex_capacity_ = 0;
}

void Ui::begin(int width, int height, float mouse_x, float mouse_y, bool mouse_down, bool pressed, bool released)
{
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    mx_ = mouse_x;
    my_ = mouse_y;
    mouse_pressed_ = pressed || (mouse_down && !was_down_);
    mouse_released_ = released || (!mouse_down && was_down_);
    mouse_down_ = mouse_down;
    was_down_ = mouse_down;
    hot_ = 0;
    verts_.clear();
    clips_.clear();
    needs_backdrop_ = false;
    typed_.clear();
    backspace_ = false;
}

void Ui::feed_text(std::string_view utf8) { typed_.append(utf8); }
void Ui::key_backspace() { backspace_ = true; }

void Ui::vertex(float px, float py, float u, float v, Color color)
{
    Vertex out{};
    out.x = (px / static_cast<float>(width_)) * 2.0f - 1.0f;
    out.y = 1.0f - (py / static_cast<float>(height_)) * 2.0f;
    out.u = u;
    out.v = v;
    out.r = color.r;
    out.g = color.g;
    out.b = color.b;
    out.a = color.a;
    out.fx_outer = 0.5f;
    out.fx_aa = 0.02f;
    out.fx_glow = 0.5f;
    out.fx_gs = 0.0f;
    verts_.push_back(out);
}

void Ui::glyph_vertex(float px, float py, float u, float v, Color color, const TextStyle& style, const TextFx& fx)
{
    Vertex out{};
    out.x = (px / static_cast<float>(width_)) * 2.0f - 1.0f;
    out.y = 1.0f - (py / static_cast<float>(height_)) * 2.0f;
    out.u = u;
    out.v = v;
    out.r = color.r;
    out.g = color.g;
    out.b = color.b;
    out.a = color.a;
    // With no outline the shader lerps between identical colors, so plain text
    // keeps the exact old blending (unpremultiplied rgb, coverage in alpha).
    const Color outline = fx.outline_outer < 0.5f ? style.outline : color;
    out.or_ = outline.r;
    out.og = outline.g;
    out.ob = outline.b;
    out.oa = outline.a;
    out.gr = style.glow.r;
    out.gg = style.glow.g;
    out.gb = style.glow.b;
    out.ga = style.glow.a;
    out.fx_outer = fx.outline_outer;
    out.fx_aa = fx.aa;
    out.fx_glow = fx.glow_outer;
    out.fx_gs = fx.glow_strength;
    verts_.push_back(out);
}

void Ui::quad(Rect rect, Color color)
{
    const float u = font_.white_u();
    const float v = font_.white_v();
    const float x0 = rect.x, y0 = rect.y, x1 = rect.x + rect.w, y1 = rect.y + rect.h;
    vertex(x0, y0, u, v, color);
    vertex(x1, y0, u, v, color);
    vertex(x1, y1, u, v, color);
    vertex(x0, y0, u, v, color);
    vertex(x1, y1, u, v, color);
    vertex(x0, y1, u, v, color);
}

void Ui::shape_vertex(float px, float py, Rect box, const BoxStyle& style, Color fill)
{
    Vertex out{};
    out.x = (px / static_cast<float>(width_)) * 2.0f - 1.0f;
    out.y = 1.0f - (py / static_cast<float>(height_)) * 2.0f;
    out.u = font_.white_u();
    out.v = font_.white_v();
    out.r = fill.r;
    out.g = fill.g;
    out.b = fill.b;
    out.a = fill.a;
    out.or_ = style.border.r;
    out.og = style.border.g;
    out.ob = style.border.b;
    out.oa = style.border.a;
    out.fx_outer = 0.5f;
    out.fx_aa = 0.02f;
    out.fx_glow = 0.5f;
    // Shape quads never glow; the slot carries the frosted-glass radius instead.
    out.fx_gs = style.backdrop_px;
    out.sx = px - box.x;
    out.sy = py - box.y;
    out.sw = box.w;
    out.sh = box.h;
    out.sr = style.radius;
    out.sb = style.border_px;
    out.sm = 1.0f;
    out.ss = style.softness_px;
    verts_.push_back(out);
}

void Ui::emit_shape(Rect quad_rect, Rect box, const BoxStyle& style, Color fill)
{
    const float x0 = quad_rect.x, y0 = quad_rect.y;
    const float x1 = quad_rect.x + quad_rect.w, y1 = quad_rect.y + quad_rect.h;
    shape_vertex(x0, y0, box, style, fill);
    shape_vertex(x1, y0, box, style, fill);
    shape_vertex(x1, y1, box, style, fill);
    shape_vertex(x0, y0, box, style, fill);
    shape_vertex(x1, y1, box, style, fill);
    shape_vertex(x0, y1, box, style, fill);
}

bool Ui::prepare_backdrop(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain,
                          std::uint32_t target_w, std::uint32_t target_h)
{
    if (!needs_backdrop_) return true;
    return backdrop_.prepare(host.device(), command.handle, swapchain, target_w, target_h);
}

void Ui::box(Rect rect, const BoxStyle& style)
{
    if (style.backdrop_px > 0.0f) needs_backdrop_ = true;
    const bool shaped = style.radius > 0.0f || style.border_px > 0.0f || style.softness_px > 0.0f
        || style.backdrop_px > 0.0f;
    const bool shadow = style.shadow.a > 0.0f && (style.shadow_blur > 0.0f || style.shadow_x != 0.0f
                                                  || style.shadow_y != 0.0f);
    if (!shaped && !shadow) {
        quad(rect, style.fill);
        return;
    }
    if (shadow) {
        const float grow = style.shadow_blur * 2.0f + std::max(std::abs(style.shadow_x), std::abs(style.shadow_y))
            + style.border_px;
        const Rect outer{rect.x - grow + style.shadow_x, rect.y - grow + style.shadow_y, rect.w + grow * 2.0f,
                         rect.h + grow * 2.0f};
        BoxStyle blur;
        blur.fill = style.shadow;
        blur.radius = style.radius;
        blur.softness_px = style.shadow_blur;
        emit_shape(outer, rect, blur, style.shadow);
    }
    BoxStyle main = style;
    main.shadow = {};
    emit_shape(rect, rect, main, style.fill);
}

void Ui::text(float x, float y, std::string_view s, Color color, float scale)
{
    const TextStyle& style = text_style_;
    const TextFx fx = resolve_text_fx(style, scale, font_.sdf_texels_per_px(), font_.sdf_units_per_texel());
    const bool shadow = style.shadow.a > 0.0f && (style.shadow_x != 0.0f || style.shadow_y != 0.0f);
    for (int pass = 0; pass < (shadow ? 2 : 1); ++pass) {
        // Silhouette first: same outline shape in the shadow color, main pass on top.
        const float dx = (pass == 0 && shadow) ? style.shadow_x * scale : 0.0f;
        const float dy = (pass == 0 && shadow) ? style.shadow_y * scale : 0.0f;
        const Color pass_color = (pass == 0 && shadow) ? style.shadow : color;
        const TextStyle pass_style = (pass == 0 && shadow) ? TextStyle{.outline = style.shadow} : style;
        const char* p = s.data();
        const char* end = p + s.size();
        float pen = x;
        const float baseline = y + font_.ascent() * scale;
        while (p < end) {
            const auto cp = utf8_next(p, end);
            if (cp == 0) break;
            const auto* g = font_.glyph(cp);
            if (!g) {
                pen += font_.size() * 0.45f * scale;
                continue;
            }
            const float x0 = pen + g->x0 * scale + dx;
            const float y0 = baseline + g->y0 * scale + dy;
            const float x1 = pen + g->x1 * scale + dx;
            const float y1 = baseline + g->y1 * scale + dy;
            glyph_vertex(x0, y0, g->u0, g->v0, pass_color, pass_style, fx);
            glyph_vertex(x1, y0, g->u1, g->v0, pass_color, pass_style, fx);
            glyph_vertex(x1, y1, g->u1, g->v1, pass_color, pass_style, fx);
            glyph_vertex(x0, y0, g->u0, g->v0, pass_color, pass_style, fx);
            glyph_vertex(x1, y1, g->u1, g->v1, pass_color, pass_style, fx);
            glyph_vertex(x0, y1, g->u0, g->v1, pass_color, pass_style, fx);
            pen += g->advance * scale;
        }
    }
}

void Ui::text_center(float cx, float y, std::string_view s, Color color, float scale)
{
    text(cx - font_.width(s, scale) * 0.5f, y, s, color, scale);
}

bool Ui::hit(Rect rect) const
{
    return mx_ >= rect.x && mx_ <= rect.x + rect.w && my_ >= rect.y && my_ <= rect.y + rect.h;
}

Color Ui::hover_fill(std::uint32_t id, bool over, bool down)
{
    if (down) return kDown;
    if (!over) return kBtn;
    if (hover_id_ != id) {
        hover_id_ = id;
        hover_since_ = time_;
    }
    const float fade = std::min(1.0f, (time_ - hover_since_) / 0.12f);
    const float k = std::max(0.35f, fade);
    return {kBtn.r + (kHover.r - kBtn.r) * k, kBtn.g + (kHover.g - kBtn.g) * k, kBtn.b + (kHover.b - kBtn.b) * k, 1.0f};
}

void Ui::styled_box(Rect rect, Color fill)
{
    BoxStyle style;
    style.fill = fill;
    style.radius = 8.0f;
    style.border = rgba(0.0f, 0.0f, 0.0f, 0.45f);
    style.border_px = 1.0f;
    box(rect, style);
}

bool Ui::button(std::uint32_t id, Rect rect, std::string_view label)
{
    const bool over = hit(rect);
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    styled_box(rect, hover_fill(id, over, active_ == id));
    const float scale = 0.72f;
    text_center(rect.x + rect.w * 0.5f, rect.y + (rect.h - font_.ascent() * scale) * 0.5f - 4.0f, label, kText, scale);
    const bool clicked = mouse_released_ && active_ == id && over;
    if (mouse_released_ && active_ == id) active_ = 0;
    return clicked;
}

bool Ui::checkbox(std::uint32_t id, Rect rect, bool* value, std::string_view label)
{
    const Rect box{rect.x, rect.y + (rect.h - 22) * 0.5f, 22, 22};
    const bool over = hit(rect);
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    styled_box(box, hover_fill(id, over, active_ == id));
    if (*value) {
        BoxStyle check;
        check.fill = rgba(0.45f, 0.85f, 0.55f);
        check.radius = 3.0f;
        this->box({box.x + 5, box.y + 5, 12, 12}, check);
    }
    text(box.x + 32, rect.y + (rect.h - font_.ascent() * 0.7f) * 0.5f, label, kText, 0.7f);
    const bool clicked = mouse_released_ && active_ == id && over;
    if (clicked) *value = !*value;
    if (mouse_released_ && active_ == id) active_ = 0;
    return clicked;
}

bool Ui::slider(std::uint32_t id, Rect rect, float* value, float min, float max, std::string_view label)
{
    text(rect.x, rect.y, label, kMuted, 0.62f);
    const Rect track{rect.x, rect.y + 28, rect.w, 10};
    const bool over = hit({rect.x, rect.y, rect.w, rect.h});
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    if (active_ == id && mouse_down_) {
        const float t = std::clamp((mx_ - track.x) / track.w, 0.0f, 1.0f);
        *value = min + t * (max - min);
    }
    BoxStyle groove;
    groove.fill = kBtn;
    groove.radius = 5.0f;
    groove.border = rgba(0.0f, 0.0f, 0.0f, 0.45f);
    groove.border_px = 1.0f;
    box(track, groove);
    const float t = (*value - min) / (max - min);
    const float fw = track.w * std::clamp(t, 0.0f, 1.0f);
    if (fw > 0.5f) {
        BoxStyle fill;
        fill.fill = rgba(0.35f, 0.72f, 0.45f);
        fill.radius = 5.0f;
        box({track.x, track.y, fw, track.h}, fill);
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", *value);
    text(rect.x + rect.w - font_.width(buf, 0.6f), rect.y, buf, kText, 0.6f);
    if (mouse_released_ && active_ == id) {
        active_ = 0;
        return true;
    }
    return false;
}

bool Ui::field(std::uint32_t id, Rect rect, char* buffer, int capacity)
{
    const bool over = hit(rect);
    if (over) hot_ = id;
    if (over && mouse_pressed_) field_active_ = id;
    if (mouse_pressed_ && !over && field_active_ == id) field_active_ = 0;
    const bool focus = field_active_ == id;
    BoxStyle frame;
    frame.fill = focus ? rgba(0.16f, 0.22f, 0.18f) : kBtn;
    frame.radius = 8.0f;
    frame.border = focus ? rgba(0.45f, 0.85f, 0.55f, 0.9f) : rgba(0.0f, 0.0f, 0.0f, 0.45f);
    frame.border_px = focus ? 2.0f : 1.0f;
    box(rect, frame);
    if (focus) {
        if (backspace_) {
            const int n = static_cast<int>(std::strlen(buffer));
            if (n > 0) buffer[n - 1] = 0;
        }
        if (!typed_.empty()) {
            const int n = static_cast<int>(std::strlen(buffer));
            const int room = capacity - 1 - n;
            if (room > 0) std::strncat(buffer, typed_.c_str(), static_cast<std::size_t>(room));
        }
    }
    text(rect.x + 12, rect.y + (rect.h - font_.ascent() * 0.7f) * 0.5f, buffer, kText, 0.7f);
    return focus;
}

bool Ui::cycle(std::uint32_t id, Rect rect, int* index, const char* const* items, int count)
{
    if (button(id, rect, items[std::clamp(*index, 0, count - 1)])) {
        *index = (*index + 1) % count;
        return true;
    }
    return false;
}

bool Ui::tabs(std::uint32_t id, Rect rect, const char* const* items, int count, int* selected)
{
    if (count <= 0) return false;
    *selected = std::clamp(*selected, 0, count - 1);
    const float w = rect.w / static_cast<float>(count);
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        const std::uint32_t tab = id * 1315423911u + static_cast<std::uint32_t>(i) + 7u;
        const Rect slot{rect.x + w * static_cast<float>(i) + 1.0f, rect.y, w - 2.0f, rect.h};
        const bool sel = i == *selected;
        const bool over = hit(slot);
        if (over) hot_ = tab;
        if (over && mouse_pressed_) active_ = tab;
        styled_box(slot, sel ? kDown : hover_fill(tab, over, active_ == tab));
        text_center(slot.x + slot.w * 0.5f, slot.y + (slot.h - font_.ascent() * 0.7f) * 0.5f - 2.0f, items[i],
                    sel ? rgba(0.95f, 0.97f, 0.93f) : kText, 0.7f);
        if (mouse_released_ && active_ == tab && over) {
            *selected = i;
            changed = true;
        }
        if (mouse_released_ && active_ == tab) active_ = 0;
    }
    return changed;
}

bool Ui::dropdown(std::uint32_t id, Rect rect, const char* const* items, int count, int* index)
{
    if (count <= 0) return false;
    *index = std::clamp(*index, 0, count - 1);
    const bool over = hit(rect);
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    const bool open = drop_open_ == id;
    styled_box(rect, hover_fill(id, over || open, active_ == id));
    text(rect.x + 12, rect.y + (rect.h - font_.ascent() * 0.7f) * 0.5f, items[*index], kText, 0.7f);
    text(rect.x + rect.w - 24, rect.y + (rect.h - font_.ascent() * 0.7f) * 0.5f, open ? "^" : "v", kMuted, 0.7f);
    if (mouse_released_ && active_ == id && over) drop_open_ = open ? 0 : id;
    if (mouse_released_ && active_ == id) active_ = 0;
    if (!open) return false;
    if (mouse_pressed_ && !over) {
        const float list_h = static_cast<float>(count) * rect.h;
        const Rect list{rect.x, rect.y + rect.h + 4.0f, rect.w, list_h};
        if (!hit(list)) drop_open_ = 0;
    }
    if (drop_open_ != id) return false;
    const float row_h = rect.h;
    const float list_h = static_cast<float>(count) * row_h;
    const bool up = rect.y + rect.h + 4.0f + list_h > static_cast<float>(height_);
    const float list_y = up ? rect.y - 4.0f - list_h : rect.y + rect.h + 4.0f;
    BoxStyle panel;
    panel.fill = rgba(0.09f, 0.12f, 0.10f, 0.88f);
    panel.radius = 8.0f;
    panel.border = rgba(0.0f, 0.0f, 0.0f, 0.5f);
    panel.border_px = 1.0f;
    panel.shadow = rgba(0.0f, 0.0f, 0.0f, 0.5f);
    panel.shadow_blur = 10.0f;
    panel.backdrop_px = 12.0f;
    box({rect.x, list_y, rect.w, list_h}, panel);
    for (int i = 0; i < count; ++i) {
        const std::uint32_t row = id * 1315423911u + static_cast<std::uint32_t>(i) + 101u;
        const Rect slot{rect.x + 4.0f, list_y + row_h * static_cast<float>(i) + 2.0f, rect.w - 8.0f, row_h - 4.0f};
        const bool row_over = hit(slot);
        if (row_over) hot_ = row;
        if (row_over && mouse_pressed_) active_ = row;
        if (i == *index || row_over) {
            BoxStyle hl;
            hl.fill = i == *index ? rgba(0.16f, 0.34f, 0.22f) : rgba(0.20f, 0.26f, 0.21f);
            hl.radius = 5.0f;
            box(slot, hl);
        }
        text(slot.x + 8.0f, slot.y + (slot.h - font_.ascent() * 0.65f) * 0.5f, items[i], kText, 0.65f);
        if (mouse_released_ && active_ == row && row_over) {
            *index = i;
            active_ = 0;
            drop_open_ = 0;
            return true;
        }
        if (mouse_released_ && active_ == row) active_ = 0;
    }
    return false;
}

void Ui::progress(Rect rect, float frac)
{
    const float f = std::clamp(frac, 0.0f, 1.0f);
    BoxStyle groove;
    groove.fill = kBtn;
    groove.radius = rect.h * 0.5f;
    groove.border = rgba(0.0f, 0.0f, 0.0f, 0.45f);
    groove.border_px = 1.0f;
    box(rect, groove);
    if (f * rect.w > 0.5f) {
        BoxStyle fill;
        fill.fill = rgba(0.35f, 0.72f, 0.45f);
        fill.radius = rect.h * 0.5f;
        box({rect.x, rect.y, rect.w * f, rect.h}, fill);
    }
}

void Ui::separator(float x0, float x1, float y)
{
    if (x1 > x0) quad({x0, y, x1 - x0, 2.0f}, rgba(1.0f, 1.0f, 1.0f, 0.10f));
}

void Ui::clip_begin(Rect rect) { clips_.push_back(ClipMark{static_cast<std::uint32_t>(verts_.size()), rect, true}); }

void Ui::clip_end() { clips_.push_back(ClipMark{static_cast<std::uint32_t>(verts_.size()), {}, false}); }

bool Ui::scrollbox(std::uint32_t id, Rect rect, float content_h, float* scroll)
{
    BoxStyle panel;
    panel.fill = rgba(0.07f, 0.09f, 0.08f, 0.94f);
    panel.radius = 10.0f;
    panel.border = rgba(1.0f, 1.0f, 1.0f, 0.07f);
    panel.border_px = 1.0f;
    box(rect, panel);
    const float pad = 10.0f;
    const float view_h = rect.h - pad * 2.0f;
    const float max = std::max(0.0f, content_h - view_h);
    *scroll = std::clamp(*scroll, 0.0f, max);
    if (max <= 0.0f) return false;
    const Rect track{rect.x + rect.w - pad - 12.0f, rect.y + pad, 12.0f, view_h};
    BoxStyle groove;
    groove.fill = rgba(0.0f, 0.0f, 0.0f, 0.35f);
    groove.radius = 6.0f;
    box(track, groove);
    const float thumb_h = std::max(30.0f, view_h * view_h / content_h);
    const float range = view_h - thumb_h;
    const float thumb_y = track.y + (range > 0.0f ? *scroll / max * range : 0.0f);
    const Rect thumb{track.x, thumb_y, track.w, thumb_h};
    const bool over = hit(thumb) || hit(track);
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    BoxStyle grip;
    grip.fill = active_ == id ? kDown : (hit(thumb) ? kHover : kBtn);
    grip.radius = 6.0f;
    box(thumb, grip);
    if (active_ != id || !mouse_down_) return false;
    if (mouse_pressed_ && hit(track) && !hit(thumb)) {
        const float next = std::clamp(*scroll + (my_ < thumb_y ? -view_h * 0.9f : view_h * 0.9f), 0.0f, max);
        const bool changed = next != *scroll;
        *scroll = next;
        return changed;
    }
    const float t = range > 0.0f ? std::clamp((my_ - track.y - thumb_h * 0.5f) / range, 0.0f, 1.0f) : 0.0f;
    const float next = t * max;
    const bool changed = next != *scroll;
    *scroll = next;
    return changed;
}

bool Ui::submit(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, bool clear, SDL_FColor clear_color,
                std::uint32_t target_w, std::uint32_t target_h)
{
    if (!pipeline_ || verts_.empty()) {
        if (!clear) return true;
        SDL_GPUColorTargetInfo color{};
        color.texture = swapchain;
        color.clear_color = clear_color;
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
        if (!pass) return false;
        SDL_EndGPURenderPass(pass);
        return true;
    }
    const auto bytes = static_cast<Uint32>(verts_.size() * sizeof(Vertex));
    if (bytes > vertex_capacity_) {
        if (vertices_) SDL_ReleaseGPUBuffer(host.device(), vertices_);
        if (transfer_) SDL_ReleaseGPUTransferBuffer(host.device(), transfer_);
        vertex_capacity_ = bytes + 4096;
        SDL_GPUBufferCreateInfo vb{};
        vb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vb.size = vertex_capacity_;
        vertices_ = SDL_CreateGPUBuffer(host.device(), &vb);
        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tb.size = vertex_capacity_;
        transfer_ = SDL_CreateGPUTransferBuffer(host.device(), &tb);
        if (!vertices_ || !transfer_) return false;
    }
    auto* map = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(host.device(), transfer_, true));
    if (!map) return false;
    std::memcpy(map, verts_.data(), bytes);
    SDL_UnmapGPUTransferBuffer(host.device(), transfer_);
    auto* copy = SDL_BeginGPUCopyPass(command.handle);
    if (!copy) return false;
    const SDL_GPUTransferBufferLocation src{transfer_, 0};
    const SDL_GPUBufferRegion dst{vertices_, 0, bytes};
    SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = clear_color;
    color.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return false;
    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    const SDL_GPUBufferBinding bind{vertices_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &bind, 1);
    const SDL_GPUTextureSamplerBinding samp{atlas_, sampler_};
    SDL_BindGPUFragmentSamplers(pass, 0, &samp, 1);
    const SDL_GPUTextureSamplerBinding blur{backdrop_.texture(), backdrop_.sampler()};
    if (blur.texture && blur.sampler) SDL_BindGPUFragmentSamplers(pass, 1, &blur, 1);
    const float target[2] = {static_cast<float>(target_w), static_cast<float>(target_h)};
    SDL_PushGPUFragmentUniformData(command.handle, 0, &target, sizeof(target));
    // Canvas units map linearly onto the target through NDC, so clip rects scale
    // by the same factor on both axes. A full-target scissor is the neutral run.
    const SDL_Rect full{0, 0, static_cast<int>(target_w), static_cast<int>(target_h)};
    for (const auto& run : clip_runs(static_cast<std::uint32_t>(verts_.size()), clips_)) {
        SDL_Rect scissor = full;
        if (run.clipped && width_ > 0 && height_ > 0 && target_w > 0 && target_h > 0) {
            const float kx = static_cast<float>(target_w) / static_cast<float>(width_);
            const float ky = static_cast<float>(target_h) / static_cast<float>(height_);
            const int x0 = std::clamp(static_cast<int>(run.rect.x * kx), 0, static_cast<int>(target_w));
            const int y0 = std::clamp(static_cast<int>(run.rect.y * ky), 0, static_cast<int>(target_h));
            const int x1 = std::clamp(static_cast<int>((run.rect.x + run.rect.w) * kx), 0, static_cast<int>(target_w));
            const int y1 = std::clamp(static_cast<int>((run.rect.y + run.rect.h) * ky), 0, static_cast<int>(target_h));
            scissor = SDL_Rect{x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
        }
        SDL_SetGPUScissor(pass, &scissor);
        const std::uint32_t count = run.end - run.start;
        if (count > 0) SDL_DrawGPUPrimitives(pass, count, 1, run.start, 0);
    }
    SDL_EndGPURenderPass(pass);
    return true;
}

} // namespace forge::ui
