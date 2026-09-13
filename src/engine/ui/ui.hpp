#pragma once

#include "engine/rhi/host.hpp"
#include "engine/ui/font.hpp"

#include <SDL3/SDL.h>
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

class Ui {
public:
    bool create(rhi::Host& host);
    void destroy(rhi::Host& host);
    void begin(int width, int height, float mouse_x, float mouse_y, bool mouse_down);
    void feed_text(std::string_view utf8);
    void key_backspace();
    bool wants_text() const { return field_active_ != 0; }

    void quad(Rect rect, Color color);
    void text(float x, float y, std::string_view s, Color color, float scale = 1.0f);
    void text_center(float cx, float y, std::string_view s, Color color, float scale = 1.0f);
    bool button(std::uint32_t id, Rect rect, std::string_view label);
    bool checkbox(std::uint32_t id, Rect rect, bool* value, std::string_view label);
    bool slider(std::uint32_t id, Rect rect, float* value, float min, float max, std::string_view label);
    bool field(std::uint32_t id, Rect rect, char* buffer, int capacity);
    bool cycle(std::uint32_t id, Rect rect, int* index, const char* const* items, int count);

    bool submit(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, bool clear, SDL_FColor clear_color);

    int width() const { return width_; }
    int height() const { return height_; }
    const Font& font() const { return font_; }

private:
    struct Vertex {
        float x, y, u, v, r, g, b, a;
    };

    void vertex(float x, float y, float u, float v, Color color);
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

} // namespace forge::ui
