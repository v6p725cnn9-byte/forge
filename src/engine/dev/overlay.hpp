#pragma once

#include "engine/core/camera.hpp"
#include "engine/render/settings.hpp"
#include <SDL3/SDL.h>

namespace forge::dev {

class Overlay {
public:
    bool init(SDL_Window* window, SDL_GPUDevice* device);
    void shutdown();
    void process_event(const SDL_Event& event);
    bool wants_mouse() const;
    bool wants_keyboard() const;
    void toggle() { visible_ = !visible_; }
    void build(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width,
               Uint32 height, Uint32 triangles, bool captured);
    void prepare(SDL_GPUCommandBuffer* command);
    void render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass);

private:
#if FORGE_DEV_UI
    bool initialized_ = false;
#endif
    bool visible_ = true;
};

} // namespace forge::dev
