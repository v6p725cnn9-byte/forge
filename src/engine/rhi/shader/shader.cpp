#include "engine/rhi/shader/shader.hpp"

#include <array>
#include <string>

namespace forge::rhi {
namespace {
struct Format {
    SDL_GPUShaderFormat flag;
    const char* extension;
    const char* entry;
};
constexpr std::array formats{
    Format{SDL_GPU_SHADERFORMAT_DXIL, ".dxil", "main"},
    Format{SDL_GPU_SHADERFORMAT_SPIRV, ".spv", "main"},
    Format{SDL_GPU_SHADERFORMAT_MSL, ".msl", "main0"},
};
bool file_exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}
}

std::filesystem::path shader_directory()
{
    const char* base = SDL_GetBasePath();
    return std::filesystem::path(base ? base : ".") / "shaders";
}

SDL_GPUShaderFormat available_shader_formats(const std::filesystem::path& directory,
                                             const std::vector<const char*>& names)
{
    SDL_GPUShaderFormat result = 0;
    for (const auto& format : formats) {
        bool complete = true;
        for (const char* name : names) {
            if (!file_exists(directory / (std::string(name) + format.extension))) {
                complete = false;
                break;
            }
        }
        if (complete) result |= format.flag;
    }
    return result;
}

SDL_GPUShader* load_shader(SDL_GPUDevice* device, const std::filesystem::path& directory,
                           const ShaderSpec& spec)
{
    const auto supported = SDL_GetGPUShaderFormats(device);
    for (const auto& format : formats) {
        if (!(supported & format.flag)) continue;
        const auto path = directory / (std::string(spec.name) + format.extension);
        if (!file_exists(path)) continue;
        size_t size = 0;
        auto* data = static_cast<Uint8*>(SDL_LoadFile(path.string().c_str(), &size));
        if (!data || size == 0) {
            SDL_Log("Cannot read shader %s: %s", path.string().c_str(), SDL_GetError());
            SDL_free(data);
            return nullptr;
        }
        SDL_GPUShaderCreateInfo info{};
        info.code = data;
        info.code_size = size;
        info.entrypoint = format.entry;
        info.format = format.flag;
        info.stage = spec.stage;
        info.num_uniform_buffers = spec.uniform_buffers;
        info.num_samplers = spec.samplers;
        info.num_storage_textures = spec.storage_textures;
        info.num_storage_buffers = spec.storage_buffers;
        SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
        SDL_free(data);
        if (!shader) SDL_Log("Shader %s failed: %s", path.string().c_str(), SDL_GetError());
        else SDL_Log("Loaded shader: %s", path.string().c_str());
        return shader;
    }
    SDL_Log("No supported shader for %s in %s (formats 0x%x)", spec.name, directory.string().c_str(),
            supported);
    return nullptr;
}

} // namespace forge::rhi
