#include "engine/app/settings.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace forge::app {
namespace {

std::string trim(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

bool truthy(const std::string& v)
{
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

} // namespace

std::filesystem::path settings_path()
{
    if (const char* env = std::getenv("FORGE_SETTINGS")) return env;
    if (char* pref = SDL_GetPrefPath("dev.forge", "forge")) {
        const std::filesystem::path path = std::filesystem::path(pref) / "settings.cfg";
        SDL_free(pref);
        return path;
    }
    return "forge-settings.cfg";
}

bool load_settings(Settings& settings, const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const auto key = trim(line.substr(0, eq));
        const auto value = trim(line.substr(eq + 1));
        if (key.empty()) continue;
        try {
            if (key == "language") settings.language = value;
            else if (key == "width") settings.width = std::max(640, std::stoi(value));
            else if (key == "height") settings.height = std::max(360, std::stoi(value));
            else if (key == "fullscreen") settings.fullscreen = truthy(value);
            else if (key == "vsync") settings.vsync = truthy(value);
            else if (key == "master") settings.master = std::clamp(std::stof(value), 0.0f, 1.0f);
            else if (key == "music") settings.music = std::clamp(std::stof(value), 0.0f, 1.0f);
            else if (key == "sfx") settings.sfx = std::clamp(std::stof(value), 0.0f, 1.0f);
            else if (key == "sensitivity") settings.sensitivity = std::clamp(std::stof(value), 0.01f, 1.0f);
            else if (key == "invert_y") settings.invert_y = truthy(value);
            else if (key == "port") settings.port = std::clamp(std::stoi(value), 1, 65535);
            else if (key == "last_join") settings.last_join = value;
        } catch (...) {
            continue;
        }
    }
    if (settings.language != "en") settings.language = "ru";
    return true;
}

bool save_settings(const Settings& settings, const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "language=" << settings.language << '\n';
    out << "width=" << settings.width << '\n';
    out << "height=" << settings.height << '\n';
    out << "fullscreen=" << (settings.fullscreen ? 1 : 0) << '\n';
    out << "vsync=" << (settings.vsync ? 1 : 0) << '\n';
    out << "master=" << settings.master << '\n';
    out << "music=" << settings.music << '\n';
    out << "sfx=" << settings.sfx << '\n';
    out << "sensitivity=" << settings.sensitivity << '\n';
    out << "invert_y=" << (settings.invert_y ? 1 : 0) << '\n';
    out << "port=" << settings.port << '\n';
    out << "last_join=" << settings.last_join << '\n';
    return static_cast<bool>(out);
}

} // namespace forge::app
