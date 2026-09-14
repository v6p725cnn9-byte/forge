#include "engine/app/lab.hpp"
#include "engine/core/scope_exit.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>

namespace forge::app {
namespace {

void capture_mouse(rhi::Host& host, bool& captured, bool enable)
{
    if (captured == enable) return;
    if (!SDL_SetWindowRelativeMouseMode(host.window(), enable)) {
        SDL_Log("Cannot change mouse capture: %s", SDL_GetError());
        return;
    }
    captured = enable;
}

} // namespace

int run_lab(Lab& lab, const char* window_title)
{
    int smoke_frames = 0;
    if (const char* env = std::getenv("FORGE_SMOKE")) {
        const std::string_view value(env);
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), smoke_frames);
        if (error != std::errc{} || end != value.data() + value.size() || smoke_frames <= 0) {
            SDL_Log("FORGE_SMOKE must be a positive frame count");
            return 1;
        }
    }
    const bool smoke_resize = smoke_frames > 0 && std::getenv("FORGE_SMOKE_RESIZE") != nullptr;

    rhi::Host host;
    Camera camera;
    if (!host.open(window_title, 1280, 720, lab.shaders())) return 1;
    ScopeExit cleanup([&] { lab.teardown(host); });
    if (!lab.setup(host, camera)) return 1;

    bool quit = false;
    bool captured = false;
    Uint64 fps_anchor = SDL_GetTicks();
    Uint64 last_progress = fps_anchor;
    Uint64 previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    int frames = 0;
    int presented = 0;
    bool resized = false;
    bool restored = false;
    while (!quit) {
        LabInput input{};
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            host.overlay().process_event(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit = true;
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) capture_mouse(host, captured, false);
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE) {
                    if (captured) capture_mouse(host, captured, false);
                    else quit = true;
                }
                if (event.key.key == SDLK_F1) host.overlay().toggle();
                if (event.key.key == SDLK_F) input.toggle_walk = true;
                if (captured && !host.overlay().wants_keyboard()) {
                    if (event.key.key == SDLK_E) input.interact = true;
                    if (event.key.key == SDLK_C) input.place = true;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT
                && !host.overlay().wants_mouse()) capture_mouse(host, captured, true);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
                capture_mouse(host, captured, false);
            if (event.type == SDL_EVENT_MOUSE_MOTION && captured) camera.look(event.motion.xrel, event.motion.yrel);
        }
        if (quit) break;

        input.captured = captured;
        if (captured && !host.overlay().wants_keyboard()
            && (SDL_GetWindowFlags(host.window()) & SDL_WINDOW_INPUT_FOCUS)) {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            input.move = {
                static_cast<float>(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]),
                static_cast<float>(keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q]),
                static_cast<float>(keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]),
            };
            input.boost = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            input.jump = keys[SDL_SCANCODE_SPACE];
        }

        const Uint64 counter = SDL_GetPerformanceCounter();
        const auto delta = static_cast<float>(static_cast<double>(counter - previous) / frequency);
        previous = counter;
        lab.update(std::min(delta, 0.1f), camera, input);

        if (smoke_resize && !resized && presented >= 10) {
            if (!SDL_SetWindowSize(host.window(), 960, 540)) return 1;
            resized = true;
        }
        if (smoke_resize && !restored && presented >= 30) {
            if (!SDL_SetWindowSize(host.window(), 1280, 720)) return 1;
            restored = true;
        }

        rhi::Command command(host.device());
        if (!command.handle) return 1;
        SDL_GPUTexture* swapchain = nullptr;
        Uint32 width = 0, height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command.handle, host.window(), &swapchain, &width, &height)) {
            SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            return 1;
        }
        command.has_swapchain = swapchain != nullptr;
        rhi::FrameResult result = rhi::FrameResult::skipped;
        if (swapchain) {
            host.overlay().build(camera, lab.debug_state(), host.backend(), width, height, lab.triangles(),
                                captured);
            host.overlay().prepare(command.handle);
            result = lab.draw(host, command, swapchain, width, height, camera, captured);
            if (result == rhi::FrameResult::failed) return 1;
            if (!host.present_overlay(command, swapchain)) return 1;
            if (!command.submit()) return 1;
            result = rhi::FrameResult::presented;
        } else {
            if (!command.submit()) return 1;
        }

        const Uint64 now = SDL_GetTicks();
        if (result != rhi::FrameResult::presented) {
            if (smoke_frames > 0 && now - last_progress > 10000) {
                SDL_Log("Smoke timed out waiting for a presented frame");
                return 1;
            }
            SDL_Delay(10);
            continue;
        }
        last_progress = now;
        ++frames;
        ++presented;
        if (smoke_frames > 0 && presented >= smoke_frames) {
            SDL_Log("Smoke passed: %d frames presented", presented);
            capture_mouse(host, captured, false);
            return 0;
        }
        if (now - fps_anchor >= 1000) {
            const auto fps = static_cast<int>(static_cast<double>(frames) * 1000.0 / static_cast<double>(now - fps_anchor));
            const std::string title = std::string(window_title) + " — " + host.backend() + " — "
                + std::to_string(fps) + " fps";
            SDL_SetWindowTitle(host.window(), title.c_str());
            fps_anchor = now;
            frames = 0;
        }
    }
    capture_mouse(host, captured, false);
    return smoke_frames > 0 ? 1 : 0;
}

} // namespace forge::app
