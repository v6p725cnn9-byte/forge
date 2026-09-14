#include "engine/platform/window/host.hpp"

#include "engine/audio/device/device.hpp"
#include "engine/core/log/log.hpp"
#include "engine/rhi/shader/shader.hpp"
#include "engine/rhi/swapchain/swapchain.hpp"

namespace forge::rhi {
namespace {

bool fail(const char* operation)
{
    forge::log::writef(forge::log::Channel::Rhi, forge::log::Level::Error, "%s failed: %s", operation, SDL_GetError());
    return false;
}

} // namespace

bool Host::open(const char* title, int width, int height, const std::vector<const char*>& shaders)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return fail("SDL_Init");
    sdl_started_ = true;
    for (int channel = 0; channel <= static_cast<int>(forge::log::Channel::Audio); ++channel)
        SDL_SetLogPriority(SDL_LOG_CATEGORY_CUSTOM + channel, SDL_LOG_PRIORITY_INFO);
    const auto formats = available_shader_formats(shader_directory(), shaders);
    if (!formats) {
        SDL_Log("No complete cooked shader set in %s; rebuild forge", shader_directory().string().c_str());
        return false;
    }
    if (!device_.create(formats, FORGE_GPU_DEBUG != 0)) return fail("SDL_CreateGPUDevice");
    forge::log::writef(forge::log::Channel::Rhi, forge::log::Level::Info, "GPU backend: %s", device_.backend());
    if (!window_.create(title, width, height)) return fail("SDL_CreateWindow");
    if (!window_.claim(device_.native())) return fail("SDL_ClaimWindowForGPUDevice");
    audio::device().open();
    if (!overlay_.init(window_.native(), device_.native())) return false;
    forge::log::writef(forge::log::Channel::Rhi, forge::log::Level::Info, "Forge %s host ready", FORGE_VERSION);
    return true;
}

bool Host::apply_display(int width, int height, bool fullscreen, bool vsync)
{
    if (!window_.native() || !device_.native()) return false;
    if (!window_.set_display(width, height, fullscreen))
        return fail(fullscreen ? "SDL_SetWindowFullscreen" : "SDL_SetWindowSize");
    if (!set_present_mode(device_.native(), window_.native(), vsync))
        forge::log::writef(forge::log::Channel::Rhi, forge::log::Level::Warn, "Swapchain present mode fallback: %s",
                           SDL_GetError());
    return true;
}

bool Host::present_overlay(Command& command, SDL_GPUTexture* swapchain)
{
    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return fail("SDL_BeginGPURenderPass (overlay)");
    overlay_.render(command.handle, pass);
    SDL_EndGPURenderPass(pass);
    return true;
}

void Host::close()
{
    audio::device().close();
    overlay_.shutdown();
    window_.destroy();
    device_.destroy();
    if (sdl_started_) SDL_Quit();
    sdl_started_ = false;
}

} // namespace forge::rhi
