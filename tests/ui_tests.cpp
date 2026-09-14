#include "engine/app/background.hpp"
#include "engine/app/menu.hpp"
#include "engine/app/menu_scene.hpp"
#include "engine/assets/scene.hpp"
#include "engine/ui/font.hpp"
#include "engine/ui/ui.hpp"

#include <stb_image.h>

#include <algorithm>
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

    // SDF atlas invariants: edge isocontour at 128, padding outside below it.
    const auto* glyph_a = font.glyph(static_cast<std::uint32_t>('A'));
    check(glyph_a->y0 < 0.0f && glyph_a->y1 > 0.0f, "SDF glyph spans the baseline");
    check(glyph_a->advance > 0.0f && font.width("A") == glyph_a->advance, "Advance matches measured width");
    check(font.width("A A") > font.width("AA"), "Space carries its own advance");
    const int atlas_w = font.atlas_width();
    const int atlas_h = font.atlas_height();
    check(atlas_w > 0 && atlas_h > 0, "SDF atlas has size");
    const auto texel = [&](float u, float v) {
        const int x = std::min(std::max(static_cast<int>(u * atlas_w), 0), atlas_w - 1);
        const int y = std::min(std::max(static_cast<int>(v * atlas_h), 0), atlas_h - 1);
        return font.pixels()[static_cast<std::size_t>(y * atlas_w + x)];
    };
    const float du = 0.5f / static_cast<float>(atlas_w);
    const float dv = 0.5f / static_cast<float>(atlas_h);
    check(texel(glyph_a->u0 + du, glyph_a->v0 + dv) < 128, "Glyph padding stays outside the edge");
    int deep = 0;
    const float vmid = (glyph_a->v0 + glyph_a->v1) * 0.5f;
    for (float u = glyph_a->u0; u < glyph_a->u1; u += du)
        deep = std::max(deep, static_cast<int>(texel(u, vmid)));
    check(deep > 140, "SDF interior sits well above the edge");
    check(texel(font.white_u(), font.white_v()) == 255, "White pixel stays solid");
    std::cout << "UI font checks passed (Выход=" << ru << " Quit=" << en << " atlas=" << atlas_w << "x" << atlas_h
              << ")\n";

    const float sdf_texels = font.sdf_texels_per_px();
    const float sdf_unit = font.sdf_units_per_texel();
    check(sdf_texels > 0.0f && sdf_unit > 0.0f, "SDF calibration exposed");
    const auto plain = forge::ui::resolve_text_fx(forge::ui::TextStyle{}, 1.0f, sdf_texels, sdf_unit);
    check(plain.outline_outer == 0.5f && plain.glow_outer == 0.5f && plain.glow_strength == 0.0f && plain.aa > 0.0f,
          "Default style keeps plain text");
    forge::ui::TextStyle outlined;
    outlined.outline = {0, 0, 0, 1};
    outlined.outline_px = 2.0f;
    const auto ol = forge::ui::resolve_text_fx(outlined, 1.0f, sdf_texels, sdf_unit);
    check(ol.outline_outer < 0.5f && ol.outline_outer >= 0.0f, "Outline pushes the outer edge out");
    forge::ui::TextStyle glowing;
    glowing.glow = {1, 0.5f, 0, 1};
    glowing.glow_px = 6.0f;
    glowing.glow_strength = 1.5f;
    const auto gl = forge::ui::resolve_text_fx(glowing, 1.0f, sdf_texels, sdf_unit);
    check(gl.glow_outer < gl.outline_outer && gl.glow_strength == 1.5f, "Glow opens a band outside the edge");
    forge::ui::TextStyle huge = glowing;
    huge.glow_px = 1000.0f;
    check(resolve_text_fx(huge, 1.0f, sdf_texels, sdf_unit).glow_outer == 0.0f, "Oversized glow clamps to range");
    check(resolve_text_fx(outlined, -1.0f, sdf_texels, sdf_unit).outline_outer == 0.5f, "Bad scale disables effects");

    forge::ui::Ui styled;
    check(styled.text_style().outline_px == 0.0f, "Style starts plain");
    forge::ui::TextStyle custom;
    custom.outline_px = 3.0f;
    custom.softness_px = 1.0f;
    styled.set_text_style(custom);
    check(styled.text_style().outline_px == 3.0f, "Style setter sticks");
    styled.begin(640, 480, 0, 0, false);
    styled.text(10, 10, "Hi", forge::ui::rgba(1, 1, 1, 1), 1.0f);
    styled.reset_text_style();
    check(styled.text_style().outline_px == 0.0f, "Style resets to plain");
    std::cout << "UI text effect checks passed\n";

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
