#pragma once

#include <SDL3/SDL.h>

namespace forge::rhi {

bool set_present_mode(SDL_GPUDevice* device, SDL_Window* window, bool vsync);

} // namespace forge::rhi
