#include "engine/ui/font.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
    const char* hello = "Привет";
    const char* p = hello;
    const char* end = hello + std::char_traits<char>::length(hello);
    check(forge::ui::utf8_next(p, end) == 0x41F, "P");
    check(forge::ui::utf8_next(p, end) == 0x440, "r");

    const auto path = std::filesystem::path("assets/fonts/DroidSans.ttf");
    const auto staged = std::filesystem::path("build/assets/fonts/DroidSans.ttf");
    forge::ui::Font font;
    const auto file = std::filesystem::exists(path) ? path : staged;
    check(font.bake(file, 32.0f), "bake DroidSans");
    const float ru = font.width("Выход");
    const float en = font.width("Quit");
    check(ru > 10.0f && en > 10.0f, "both scripts have width");
    check(font.glyph(0x42F) != nullptr, "Cyrillic capital Ya");
    check(font.glyph(static_cast<std::uint32_t>('A')) != nullptr, "Latin A");
    std::cout << "UI font checks passed (Выход=" << ru << " Quit=" << en << ")\n";
}
