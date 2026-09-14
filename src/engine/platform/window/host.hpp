#pragma once

#include "engine/dev/overlay.hpp"
#include "engine/platform/window/window.hpp"
#include "engine/rhi/command/command.hpp"
#include "engine/rhi/device/device.hpp"

#include <SDL3/SDL.h>
#include <vector>

namespace forge::rhi {

enum class FrameResult { presented, skipped, failed };

// Composes platform::Window + rhi::Device. Scene targets live in render::Renderer.
class Host {
public:
    Host() = default;
    ~Host() { close(); }
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    bool open(const char* title, int width, int height, const std::vector<const char*>& shaders);
    void close();
    bool apply_display(int width, int height, bool fullscreen, bool vsync);

    SDL_Window* window() const { return window_.native(); }
    SDL_GPUDevice* device() const { return device_.native(); }
    const char* backend() const { return device_.backend(); }
    platform::Window& native_window() { return window_; }
    Device& gpu() { return device_; }
    dev::Overlay& overlay() { return overlay_; }

    bool present_overlay(Command& command, SDL_GPUTexture* swapchain);

private:
    platform::Window window_;
    Device device_;
    dev::Overlay overlay_;
    bool sdl_started_ = false;
};

} // namespace forge::rhi
