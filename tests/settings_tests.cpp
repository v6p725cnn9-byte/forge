#include "engine/app/i18n.hpp"
#include "engine/app/settings.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    const auto path = std::filesystem::temp_directory_path() / "forge-settings-test.cfg";
    forge::app::Settings out;
    out.language = "en";
    out.width = 1920;
    out.height = 1080;
    out.fullscreen = true;
    out.vsync = false;
    out.master = 0.4f;
    out.music = 0.2f;
    out.sfx = 0.9f;
    out.sensitivity = 0.2f;
    out.invert_y = true;
    out.port = 27016;
    out.last_join = "10.0.0.2:27016";
    check(forge::app::save_settings(out, path), "save");
    forge::app::Settings in;
    check(forge::app::load_settings(in, path), "load");
    check(in.language == "en" && in.width == 1920 && in.height == 1080, "graphics");
    check(in.fullscreen && !in.vsync && in.invert_y, "flags");
    check(in.port == 27016 && in.last_join == "10.0.0.2:27016", "net");
    check(std::abs(in.master - 0.4f) < 0.001f && std::abs(in.sensitivity - 0.2f) < 0.001f, "floats");
    check(std::string(forge::app::tr("ru", "single")) != forge::app::tr("en", "single"), "i18n");
    check(std::string(forge::app::tr("en", "quit")) == "Quit", "english quit");
    {
        std::ofstream corrupt(path);
        corrupt << "sensitivity=nan\nmaster=inf\nwidth=1920junk\nheight=999999\nvsync=typo\nport=32oops\n";
    }
    forge::app::Settings defaults;
    auto validated = defaults;
    check(forge::app::load_settings(validated, path), "load partially corrupt config");
    check(validated.sensitivity == defaults.sensitivity && validated.master == defaults.master,
          "non-finite settings retain defaults");
    check(validated.width == defaults.width && validated.port == defaults.port && validated.vsync == defaults.vsync,
          "malformed values retain defaults");
    check(validated.height == 4320, "oversized display bounded");
    std::filesystem::remove(path);
    std::cout << "Settings checks passed\n";
}
