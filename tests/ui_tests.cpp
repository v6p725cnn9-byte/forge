#include "engine/app/background.hpp"
#include "engine/app/menu.hpp"
#include "engine/app/menu_scene.hpp"
#include "engine/assets/scene.hpp"
#include "engine/ui/font.hpp"
#include "engine/ui/ui.hpp"

#include <stb_image.h>

#include <cmath>
#include <cstddef>
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

    forge::ui::Ui input_ui;
    input_ui.begin(1280, 720, 20, 20, false, true, true);
    check(input_ui.button(1, {10, 10, 100, 40}, ""), "press and release between frames must register");
    input_ui.begin(1280, 720, 20, 20, false);
    check(!input_ui.button(1, {10, 10, 100, 40}, ""), "click must not repeat next frame");

    forge::app::Settings menu_settings;
    forge::app::Menu menu(menu_settings);
    input_ui.begin(1280, 720, 640, 225, false, true, true);
    check(menu.draw(input_ui).command == forge::app::MenuCommand::Solo, "menu solo click selects solo");
    input_ui.begin(1280, 720, 640, 341, false, true, true);
    check(menu.draw(input_ui).command == forge::app::MenuCommand::None && menu.consume_escape(),
          "settings click opens a subpage");
    input_ui.begin(1280, 720, 640, 411, false, true, true);
    check(menu.draw(input_ui).command == forge::app::MenuCommand::Quit, "menu quit click selects quit");

    using forge::app::cover_crop;
    auto crop = cover_crop(1671, 941, 1671, 941);
    check(crop.x == 0 && crop.y == 0 && crop.w == 1671 && crop.h == 941, "same aspect keeps full frame");
    crop = cover_crop(1671, 941, 1280, 720);
    check(crop.x == 0 && crop.w == 1671 && crop.h < 941 && crop.y == (941 - crop.h) / 2, "wider dst crops top/bottom");
    const auto wide_err = static_cast<std::int64_t>(crop.w) * 720 - static_cast<std::int64_t>(1280) * crop.h;
    check(wide_err >= -1280 && wide_err <= 1280, "wider dst keeps aspect within a pixel");
    crop = cover_crop(1671, 941, 900, 1600);
    check(crop.y == 0 && crop.h == 941 && crop.w < 1671 && crop.x == (1671 - crop.w) / 2, "taller dst crops sides");
    const auto tall_err = static_cast<std::int64_t>(crop.w) * 1600 - static_cast<std::int64_t>(900) * crop.h;
    check(tall_err >= -1600 && tall_err <= 1600, "taller dst keeps aspect within a pixel");
    crop = cover_crop(0, 941, 1280, 720);
    check(crop.w == 0 && crop.h == 0, "zero source is empty");
    crop = cover_crop(1671, 941, 0, 0);
    check(crop.w == 0 && crop.h == 0, "zero destination is empty");
    std::cout << "Menu cover-crop checks passed\n";

    const auto menu_path = std::filesystem::path("assets/menu/background.png");
    const auto menu_staged = std::filesystem::path("build/assets/menu/background.png");
    const auto menu_file = std::filesystem::exists(menu_path) ? menu_path : menu_staged;
    int menu_w = 0, menu_h = 0, menu_c = 0;
    check(stbi_info(menu_file.string().c_str(), &menu_w, &menu_h, &menu_c) != 0, "menu background decodes");
    check(menu_w == 1671 && menu_h == 941, "menu background is 1671x941");
    check(menu_c >= 3, "menu background has color channels");
    std::cout << "Menu background checks passed (" << menu_w << "x" << menu_h << " c=" << menu_c << ")\n";

    forge::rhi::Host closed;
    forge::app::MenuScene scene;
    check(!scene.ready(), "scene starts not ready");
    check(!scene.create(closed), "scene refuses a closed device");
    check(!scene.ready(), "failed scene stays not ready");
    scene.destroy(closed);
    std::cout << "Menu scene fallback checks passed\n";

    const auto vista = forge::assets::make_vista_terrain(forge::render::sun_direction(8.0f, 9.0f));
    check(vista.vertices.size() > 20000, "vista has ground vertices");
    check(vista.indices.size() % 3 == 0, "vista indices form triangles");
    check(vista.primitives.size() == 3, "vista has ground, water and sky");
    check(vista.materials.size() == 3, "vista has three materials");
    check(vista.materials[2].unlit, "sky is unlit");
    check(vista.bounds_min.y < -2.0f, "lake basin is carved");
    check(vista.bounds_max.y > 30.0f, "ridge ring rises");
    float normal_len = 0.0f;
    for (std::size_t i = 0; i < vista.vertices.size(); i += 977) {
        const auto& n = vista.vertices[i].normal;
        normal_len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        check(normal_len > 0.99f && normal_len < 1.01f, "vista normals are unit");
    }
    std::cout << "Vista terrain checks passed (" << vista.vertices.size() << " verts, "
              << vista.indices.size() / 3 << " tris)\n";
}
