#pragma once

#include <filesystem>
#include <string>

namespace forge::app {

struct Settings {
    std::string language = "ru";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    float master = 1.0f;
    float music = 0.8f;
    float sfx = 1.0f;
    float sensitivity = 0.12f;
    bool invert_y = false;
    int port = 27015;
    std::string last_join = "127.0.0.1:27015";
};

std::filesystem::path settings_path();
bool load_settings(Settings& settings, const std::filesystem::path& path);
bool save_settings(const Settings& settings, const std::filesystem::path& path);
inline bool load_settings(Settings& settings) { return load_settings(settings, settings_path()); }
inline bool save_settings(const Settings& settings) { return save_settings(settings, settings_path()); }

} // namespace forge::app
