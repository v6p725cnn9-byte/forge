#pragma once

#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstdio>

namespace forge::log {

enum class Channel {
    Core = 0,
    Rhi,
    Render,
    Asset,
    Anim,
    Phys,
    Game,
    Net,
    Script,
    Audio,
};

enum class Level { Trace, Info, Warn, Error };

inline int sdl_category(Channel channel)
{
    return SDL_LOG_CATEGORY_CUSTOM + static_cast<int>(channel);
}

inline void write(Channel channel, Level level, const char* message)
{
    const int category = sdl_category(channel);
    switch (level) {
    case Level::Trace:
        SDL_LogVerbose(category, "%s", message);
        break;
    case Level::Info:
        SDL_LogInfo(category, "%s", message);
        break;
    case Level::Warn:
        SDL_LogWarn(category, "%s", message);
        break;
    case Level::Error:
        SDL_LogError(category, "%s", message);
        break;
    }
}

inline void writef(Channel channel, Level level, const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    write(channel, level, buffer);
}

inline void info(const char* message) { write(Channel::Core, Level::Info, message); }
inline void warn(const char* message) { write(Channel::Core, Level::Warn, message); }
inline void error(const char* message) { write(Channel::Core, Level::Error, message); }

} // namespace forge::log
