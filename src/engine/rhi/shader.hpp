#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <initializer_list>
#include <vector>

namespace forge::rhi {

struct ShaderSpec {
    const char* name;
    SDL_GPUShaderStage stage;
    Uint32 uniform_buffers = 0;
    Uint32 samplers = 0;
    Uint32 storage_textures = 0;
    Uint32 storage_buffers = 0;
};

std::filesystem::path shader_directory();
SDL_GPUShaderFormat available_shader_formats(const std::filesystem::path& directory,
                                             const std::vector<const char*>& names);
inline SDL_GPUShaderFormat available_shader_formats(const std::filesystem::path& directory,
                                                    std::initializer_list<const char*> names)
{
    return available_shader_formats(directory, std::vector<const char*>(names));
}
SDL_GPUShader* load_shader(SDL_GPUDevice* device, const std::filesystem::path& directory,
                           const ShaderSpec& spec);

} // namespace forge::rhi
