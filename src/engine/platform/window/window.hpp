#pragma once

#include <SDL3/SDL.h>

namespace forge::platform {

class Window {
public:
    Window() = default;
    ~Window() { destroy(); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const char* title, int width, int height)
    {
        destroy();
        handle_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!handle_) return false;
        SDL_SetWindowPosition(handle_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        return true;
    }

    bool claim(SDL_GPUDevice* device)
    {
        if (!handle_ || !device) return false;
        if (!SDL_ClaimWindowForGPUDevice(device, handle_)) return false;
        device_ = device;
        claimed_ = true;
        return true;
    }

    void release()
    {
        if (claimed_ && device_ && handle_) SDL_ReleaseWindowFromGPUDevice(device_, handle_);
        claimed_ = false;
        device_ = nullptr;
    }

    void destroy()
    {
        release();
        if (handle_) SDL_DestroyWindow(handle_);
        handle_ = nullptr;
    }

    bool set_display(int width, int height, bool fullscreen)
    {
        if (!handle_) return false;
        if (fullscreen) return SDL_SetWindowFullscreen(handle_, true);
        if (!SDL_SetWindowFullscreen(handle_, false)) return false;
        return SDL_SetWindowSize(handle_, width, height);
    }

    SDL_Window* native() const { return handle_; }

private:
    SDL_Window* handle_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    bool claimed_ = false;
};

} // namespace forge::platform
