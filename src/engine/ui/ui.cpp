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

    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"ui.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"ui.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        return false;
    }
    SDL_GPUVertexBufferDescription buffer{};
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
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
    info.vertex_input_state.num_vertex_attributes = 3;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline_ && sampler_ && atlas_;
}

void Ui::destroy(rhi::Host& host)
{
    if (!host.device()) return;
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
    typed_.clear();
    backspace_ = false;
}

void Ui::feed_text(std::string_view utf8) { typed_.append(utf8); }
void Ui::key_backspace() { backspace_ = true; }

void Ui::vertex(float px, float py, float u, float v, Color color)
{
    Vertex out;
    out.x = (px / static_cast<float>(width_)) * 2.0f - 1.0f;
    out.y = 1.0f - (py / static_cast<float>(height_)) * 2.0f;
    out.u = u;
    out.v = v;
    out.r = color.r;
    out.g = color.g;
    out.b = color.b;
    out.a = color.a;
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

void Ui::text(float x, float y, std::string_view s, Color color, float scale)
{
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
        const float x0 = pen + g->x0 * scale;
        const float y0 = baseline + g->y0 * scale;
        const float x1 = pen + g->x1 * scale;
        const float y1 = baseline + g->y1 * scale;
        vertex(x0, y0, g->u0, g->v0, color);
        vertex(x1, y0, g->u1, g->v0, color);
        vertex(x1, y1, g->u1, g->v1, color);
        vertex(x0, y0, g->u0, g->v0, color);
        vertex(x1, y1, g->u1, g->v1, color);
        vertex(x0, y1, g->u0, g->v1, color);
        pen += g->advance * scale;
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

bool Ui::button(std::uint32_t id, Rect rect, std::string_view label)
{
    const bool over = hit(rect);
    if (over) hot_ = id;
    if (over && mouse_pressed_) active_ = id;
    Color fill = kBtn;
    if (active_ == id) fill = kDown;
    else if (over) fill = kHover;
    quad(rect, fill);
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
    quad(box, over ? kHover : kBtn);
    if (*value) quad({box.x + 5, box.y + 5, 12, 12}, rgba(0.45f, 0.85f, 0.55f));
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
    quad(track, kBtn);
    const float t = (*value - min) / (max - min);
    quad({track.x, track.y, track.w * std::clamp(t, 0.0f, 1.0f), track.h}, rgba(0.35f, 0.72f, 0.45f));
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
    quad(rect, focus ? rgba(0.16f, 0.22f, 0.18f) : kBtn);
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

bool Ui::submit(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, bool clear, SDL_FColor clear_color)
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
    SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(verts_.size()), 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}

} // namespace forge::ui
