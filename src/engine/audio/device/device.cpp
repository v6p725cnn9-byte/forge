#include "engine/audio/device/device.hpp"

#include "engine/core/log/log.hpp"

#include <SDL3/SDL.h>

namespace forge::audio {

bool Device::open()
{
    if (ready_) return true;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        forge::log::writef(forge::log::Channel::Audio, forge::log::Level::Warn, "SDL audio unavailable: %s",
                           SDL_GetError());
        return false;
    }
    ready_ = true;
    return true;
}

void Device::close() { ready_ = false; }

Device& device()
{
    static Device instance;
    return instance;
}

} // namespace forge::audio
