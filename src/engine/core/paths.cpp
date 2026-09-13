#include "engine/core/paths.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>

namespace forge {

std::filesystem::path executable_directory()
{
    const char* base = SDL_GetBasePath();
    return std::filesystem::path(base ? base : ".");
}

std::filesystem::path assets_directory()
{
    if (const char* env = std::getenv("FORGE_ASSETS")) {
        return std::filesystem::path(env);
    }
    std::error_code error;
    const auto next_to_binary = executable_directory() / "assets";
    if (std::filesystem::is_directory(next_to_binary, error)) return next_to_binary;
    const auto cwd = std::filesystem::current_path() / "assets";
    if (std::filesystem::is_directory(cwd, error)) return cwd;
    return next_to_binary;
}

std::filesystem::path scripts_directory()
{
    if (const char* env = std::getenv("FORGE_SCRIPTS")) {
        return std::filesystem::path(env);
    }
    std::error_code error;
    const auto next_to_binary = executable_directory() / "scripts";
    if (std::filesystem::is_directory(next_to_binary, error)) return next_to_binary;
    const auto cwd = std::filesystem::current_path() / "scripts";
    if (std::filesystem::is_directory(cwd, error)) return cwd;
    const auto assets_scripts = assets_directory() / "scripts";
    if (std::filesystem::is_directory(assets_scripts, error)) return assets_scripts;
    return next_to_binary;
}

} // namespace forge
