#pragma once

#include <SDL3/SDL.h>

namespace forge::log {

inline void info(const char* message) { SDL_Log("%s", message); }
inline void warn(const char* message) { SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message); }
inline void error(const char* message) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message); }

} // namespace forge::log
