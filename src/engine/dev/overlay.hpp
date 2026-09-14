#pragma once

#include "engine/core/camera/camera.hpp"
#include "engine/render/renderer/settings.hpp"
#include <SDL3/SDL.h>

namespace forge::dev {

// Debug-only ImGui overlay. Shipping UI is forge::ui — ImGui must never
// implement menus, inventory, HUD, or settings. FORGE_DEV_UI=0 strips this.

class Overlay {
public:
    bool init(SDL_Window* window, SDL_GPUDevice* device);
    void shutdown();
    void process_event(const SDL_Event& event);
    bool wants_mouse() const;
    bool wants_keyboard() const;
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    bool ready() const { return initialized_; }
    void begin_frame();
    void end_frame();
    void draw_debug(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width,
                    Uint32 height, Uint32 triangles, bool captured);
    void draw_hud(const forge::render::DebugState& debug);
    void draw_world_labels(const Camera& camera, const forge::render::DebugState& debug, Uint32 width, Uint32 height);
    void build(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width, Uint32 height,
               Uint32 triangles, bool captured);
    void prepare(SDL_GPUCommandBuffer* command);
    void render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass);

private:
    bool initialized_ = false;
    bool visible_ = true;
    bool frame_open_ = false;
};

} // namespace forge::dev
