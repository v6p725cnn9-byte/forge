#include "engine/app/lab.hpp"
#include "engine/app/menu.hpp"
#include "engine/app/settings.hpp"
#include "engine/ui/ui.hpp"

#include "lab.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

const std::vector<const char*> kShaders{"pbr.vert", "pbr.frag", "tonemap.vert", "tonemap.frag", "bloom.frag", "ui.vert",
                                        "ui.frag"};

} // namespace

int run_game(const char* window_title)
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

    Settings settings;
    load_settings(settings);

    rhi::Host host;
    if (!host.open(window_title, settings.width, settings.height, kShaders)) return 1;
    host.apply_display(settings.width, settings.height, settings.fullscreen, settings.vsync);
    ui::Ui game_ui;
    if (!game_ui.create(host)) return 1;
    if (host.overlay().visible()) host.overlay().toggle();

    Menu menu(settings);
    Settings applied = settings;
    std::unique_ptr<labs::SurvivalLab> session;
    Camera camera;
    camera.sensitivity = settings.sensitivity;
    camera.invert_y = settings.invert_y;

    enum class Mode { Menu, Play };
    Mode mode = Mode::Menu;
    labs::SessionLaunch launch{};

    const bool skip_menu = smoke_frames > 0 || std::getenv("FORGE_CONNECT") != nullptr;
    if (skip_menu) {
        launch.hosting = std::getenv("FORGE_CONNECT") == nullptr;
        launch.lan = false;
        if (const char* env = std::getenv("FORGE_CONNECT")) launch.connect = env;
        mode = Mode::Play;
    }

    bool quit = false;
    bool captured = false;
    bool pending_backspace = false;
    std::string pending_text;
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
        pending_text.clear();
        pending_backspace = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (mode == Mode::Play) host.overlay().process_event(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit = true;
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) capture_mouse(host, captured, false);
            if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text) pending_text += event.text.text;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_BACKSPACE) pending_backspace = true;
                if (event.key.key == SDLK_ESCAPE) {
                    if (mode == Mode::Play && captured) capture_mouse(host, captured, false);
                    else if (mode == Mode::Play) {
                        capture_mouse(host, captured, false);
                        if (session) {
                            session->teardown(host);
                            session.reset();
                        }
                        menu.reset();
                        mode = Mode::Menu;
                    } else if (!menu.consume_escape()) {
                        quit = true;
                    }
                }
                if (mode == Mode::Play && event.key.key == SDLK_F1) host.overlay().toggle();
                if (mode == Mode::Play && event.key.key == SDLK_F) input.toggle_walk = true;
                if (mode == Mode::Play && captured && !host.overlay().wants_keyboard()) {
                    if (event.key.key == SDLK_E) input.interact = true;
                    if (event.key.key == SDLK_C) input.place = true;
                }
            }
            if (mode == Mode::Play && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                && event.button.button == SDL_BUTTON_RIGHT && !host.overlay().wants_mouse())
                capture_mouse(host, captured, true);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
                capture_mouse(host, captured, false);
            if (event.type == SDL_EVENT_MOUSE_MOTION && captured) camera.look(event.motion.xrel, event.motion.yrel);
        }
        if (quit) break;

        if (mode == Mode::Menu) {
            rhi::Command command(host.device());
            if (!command.handle) return 1;
            SDL_GPUTexture* swapchain = nullptr;
            Uint32 width = 0, height = 0;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(command.handle, host.window(), &swapchain, &width, &height))
                return 1;
            command.has_swapchain = swapchain != nullptr;
            if (!swapchain) {
                command.submit();
                continue;
            }
            float mx = 0, my = 0;
            const auto buttons = SDL_GetMouseState(&mx, &my);
            int ww = 1, wh = 1;
            SDL_GetWindowSize(host.window(), &ww, &wh);
            mx *= static_cast<float>(width) / static_cast<float>(std::max(ww, 1));
            my *= static_cast<float>(height) / static_cast<float>(std::max(wh, 1));
            game_ui.begin(static_cast<int>(width), static_cast<int>(height), mx, my,
                          (buttons & SDL_BUTTON_LMASK) != 0);
            if (!pending_text.empty()) game_ui.feed_text(pending_text);
            if (pending_backspace) game_ui.key_backspace();
            if (game_ui.wants_text()) SDL_StartTextInput(host.window());
            else SDL_StopTextInput(host.window());
            const auto result = menu.draw(game_ui);
            if (!game_ui.submit(host, command, swapchain, true, {0.04f, 0.06f, 0.05f, 1.0f})) return 1;
            if (!command.submit()) return 1;
            if (settings.width != applied.width || settings.height != applied.height
                || settings.fullscreen != applied.fullscreen || settings.vsync != applied.vsync) {
                host.apply_display(settings.width, settings.height, settings.fullscreen, settings.vsync);
                applied = settings;
            }
            ++presented;
            ++frames;
            if (result.command == MenuCommand::Quit) quit = true;
            if (result.command == MenuCommand::Solo || result.command == MenuCommand::Host
                || result.command == MenuCommand::Join) {
                launch = {};
                launch.port = settings.port;
                if (result.command == MenuCommand::Solo) {
                    launch.hosting = true;
                    launch.lan = false;
                } else if (result.command == MenuCommand::Host) {
                    launch.hosting = true;
                    launch.lan = true;
                } else {
                    launch.hosting = false;
                    launch.connect = result.join;
                    settings.last_join = result.join;
                    save_settings(settings);
                }
                camera.sensitivity = settings.sensitivity;
                camera.invert_y = settings.invert_y;
                mode = Mode::Play;
            }
            if (smoke_frames > 0 && presented >= smoke_frames) {
                SDL_Log("Smoke passed: %d frames presented", presented);
                return 0;
            }
            continue;
        }

        if (!session) {
            session = std::make_unique<labs::SurvivalLab>();
            session->configure(launch);
            if (!session->setup(host, camera)) {
                SDL_Log("Failed to start session");
                session.reset();
                mode = Mode::Menu;
                menu.reset();
                continue;
            }
            camera.sensitivity = settings.sensitivity;
            camera.invert_y = settings.invert_y;
        }

        session->debug_state().language = settings.language.c_str();
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
        session->update(std::min(delta, 0.1f), camera, input);

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
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command.handle, host.window(), &swapchain, &width, &height)) return 1;
        command.has_swapchain = swapchain != nullptr;
        rhi::FrameResult frame = rhi::FrameResult::skipped;
        if (swapchain) {
            frame = session->draw(host, command, swapchain, width, height, camera, captured);
            if (frame == rhi::FrameResult::failed) return 1;
            float mx = 0, my = 0;
            const auto buttons = SDL_GetMouseState(&mx, &my);
            int ww = 1, wh = 1;
            SDL_GetWindowSize(host.window(), &ww, &wh);
            mx *= static_cast<float>(width) / static_cast<float>(std::max(ww, 1));
            my *= static_cast<float>(height) / static_cast<float>(std::max(wh, 1));
            game_ui.begin(static_cast<int>(width), static_cast<int>(height), mx, my,
                          !captured && (buttons & SDL_BUTTON_LMASK) != 0);
            draw_game_hud(game_ui, session->debug_state());
            draw_world_captions(game_ui, camera, session->debug_state(), static_cast<int>(width),
                                static_cast<int>(height));
            if (!game_ui.submit(host, command, swapchain, false, {})) return 1;
            if (host.overlay().visible()) {
                host.overlay().begin_frame();
                host.overlay().draw_debug(camera, session->debug_state(), host.backend(), width, height,
                                          session->triangles(), captured);
                host.overlay().end_frame();
                host.overlay().prepare(command.handle);
                if (!host.present_overlay(command, swapchain)) return 1;
            }
            if (!command.submit()) return 1;
            frame = rhi::FrameResult::presented;
        } else {
            command.submit();
        }

        const Uint64 now = SDL_GetTicks();
        if (frame != rhi::FrameResult::presented) {
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
            session->teardown(host);
            return 0;
        }
        if (now - fps_anchor >= 1000) {
            const auto fps = static_cast<int>(static_cast<double>(frames) * 1000.0 / static_cast<double>(now - fps_anchor));
            SDL_SetWindowTitle(host.window(), (std::string(window_title) + " — " + host.backend() + " — "
                                               + std::to_string(fps) + " fps")
                                                  .c_str());
            fps_anchor = now;
            frames = 0;
        }
    }

    capture_mouse(host, captured, false);
    if (session) session->teardown(host);
    game_ui.destroy(host);
    save_settings(settings);
    return smoke_frames > 0 ? 1 : 0;
}

} // namespace forge::app
