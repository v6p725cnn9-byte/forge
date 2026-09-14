#pragma once

#include "engine/rhi/texture/texture.hpp"

#include <string>

namespace forge::render {

struct Ibl {
    rhi::Texture irradiance;
    rhi::Texture specular;
    rhi::Texture brdf;
    Uint32 specular_mips = 1;
};

bool create_studio_ibl(SDL_GPUDevice* device, Ibl& ibl, std::string& error);
void destroy_ibl(SDL_GPUDevice* device, Ibl& ibl);

} // namespace forge::render
