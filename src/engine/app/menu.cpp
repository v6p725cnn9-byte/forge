#include "engine/app/menu.hpp"

#include "engine/app/i18n.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <glm/glm.hpp>

namespace forge::app {
namespace {

const char* t(const Settings& s, std::string_view key) { return tr(s.language, key); }

ui::Rect center_column(const ui::Ui& ui, float y, float h)
{
    const float w = std::min(440.0f, static_cast<float>(ui.width()) - 80.0f);
    const float x = (static_cast<float>(ui.width()) - w) * 0.5f;
    return {x, y, w, h};
}

} // namespace

Menu::Menu(Settings& settings) : settings_(settings), draft_(settings)
{
    std::snprintf(join_, sizeof(join_), "%s", settings_.last_join.c_str());
    std::snprintf(port_, sizeof(port_), "%d", settings_.port);
}

void Menu::reset()
{
    page_ = Page::Root;
    draft_ = settings_;
    std::snprintf(join_, sizeof(join_), "%s", settings_.last_join.c_str());
    std::snprintf(port_, sizeof(port_), "%d", settings_.port);
}

bool Menu::consume_escape()
{
    if (page_ == Page::Root) return false;
    if (page_ == Page::Graphics || page_ == Page::Controls || page_ == Page::Sound || page_ == Page::Language)
        page_ = Page::Settings;
    else
        page_ = Page::Root;
    return true;
}

MenuResult Menu::draw(ui::Ui& ui)
{
    MenuResult result;
    const auto title = t(settings_, "title");
    ui::TextStyle title_style;
    title_style.outline = ui::rgba(0.02f, 0.03f, 0.02f, 0.9f);
    title_style.outline_px = 2.0f;
    title_style.shadow = ui::rgba(0.0f, 0.0f, 0.0f, 0.55f);
    title_style.shadow_x = 0.0f;
    title_style.shadow_y = 3.0f;
    ui.set_text_style(title_style);
    ui.text_center(static_cast<float>(ui.width()) * 0.5f, 70.0f, title, ui::rgba(0.92f, 0.88f, 0.55f), 1.85f);
    ui.text_center(static_cast<float>(ui.width()) * 0.5f, 140.0f, t(settings_, "subtitle"), ui::rgba(0.65f, 0.68f, 0.62f),
                   0.75f);
    ui.reset_text_style();

    float y = 200.0f;
    auto row = [&](float height = 50.0f) {
        const auto r = center_column(ui, y, height);
        y += height + 8.0f;
        return r;
    };

    if (page_ == Page::Root) {
        if (ui.button(ui::hash_id("solo"), row(), t(settings_, "single"))) result.command = MenuCommand::Solo;
        if (ui.button(ui::hash_id("multi"), row(), t(settings_, "multi"))) page_ = Page::Multiplayer;
        if (ui.button(ui::hash_id("settings"), row(), t(settings_, "settings"))) page_ = Page::Settings;
        y += 12;
        if (ui.button(ui::hash_id("quit"), row(), t(settings_, "quit"))) result.command = MenuCommand::Quit;
    } else if (page_ == Page::Multiplayer) {
        ui.text(center_column(ui, y, 30).x, y, t(settings_, "multi"), ui::rgba(0.9f, 0.9f, 0.85f), 0.8f);
        y += 40;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "port"), ui::rgba(0.7f, 0.72f, 0.68f), 0.6f);
        y += 28;
        if (ui.field(ui::hash_id("port"), row(44), port_, sizeof(port_))) {
            settings_.port = std::clamp(std::atoi(port_), 1, 65535);
        }
        if (ui.button(ui::hash_id("host"), row(), t(settings_, "host"))) {
            settings_.port = std::clamp(std::atoi(port_), 1, 65535);
            result.command = MenuCommand::Host;
        }
        y += 8;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "address"), ui::rgba(0.7f, 0.72f, 0.68f), 0.6f);
        y += 28;
        ui.field(ui::hash_id("join"), row(44), join_, sizeof(join_));
        if (ui.button(ui::hash_id("dojoin"), row(), t(settings_, "join"))) {
            result.command = MenuCommand::Join;
            result.join = join_;
            settings_.last_join = join_;
        }
        y += 16;
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) page_ = Page::Root;
    } else if (page_ == Page::Settings) {
        if (ui.button(ui::hash_id("gfx"), row(), t(settings_, "graphics"))) {
            draft_ = settings_;
            page_ = Page::Graphics;
        }
        if (ui.button(ui::hash_id("ctl"), row(), t(settings_, "controls"))) page_ = Page::Controls;
        if (ui.button(ui::hash_id("snd"), row(), t(settings_, "sound"))) page_ = Page::Sound;
        if (ui.button(ui::hash_id("lang"), row(), t(settings_, "language"))) page_ = Page::Language;
        y += 16;
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) page_ = Page::Root;
    } else if (page_ == Page::Graphics) {
        ui.text(center_column(ui, y, 30).x, y, t(settings_, "graphics"), ui::rgba(0.9f, 0.9f, 0.85f), 0.8f);
        y += 40;
        static const char* presets[] = {"1280 x 720", "1600 x 900", "1920 x 1080", "2560 x 1440"};
        const int widths[] = {1280, 1600, 1920, 2560};
        const int heights[] = {720, 900, 1080, 1440};
        int preset = 0;
        for (int i = 0; i < 4; ++i)
            if (draft_.width == widths[i] && draft_.height == heights[i]) preset = i;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "resolution"), ui::rgba(0.7f, 0.72f, 0.68f), 0.6f);
        y += 28;
        if (ui.cycle(ui::hash_id("res"), row(), &preset, presets, 4)) {
            draft_.width = widths[preset];
            draft_.height = heights[preset];
        }
        ui.checkbox(ui::hash_id("fs"), row(40), &draft_.fullscreen, t(settings_, "fullscreen"));
        ui.checkbox(ui::hash_id("vs"), row(40), &draft_.vsync, t(settings_, "vsync"));
        if (ui.button(ui::hash_id("apply"), row(), t(settings_, "apply"))) {
            settings_.width = draft_.width;
            settings_.height = draft_.height;
            settings_.fullscreen = draft_.fullscreen;
            settings_.vsync = draft_.vsync;
            save_settings(settings_);
        }
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) page_ = Page::Settings;
    } else if (page_ == Page::Controls) {
        ui.slider(ui::hash_id("sens"), row(56), &settings_.sensitivity, 0.02f, 0.40f, t(settings_, "sensitivity"));
        ui.checkbox(ui::hash_id("inv"), row(40), &settings_.invert_y, t(settings_, "invert_y"));
        y += 8;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_move"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 28;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_look"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 28;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_run"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 28;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_use"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 28;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_craft"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 28;
        ui.text(center_column(ui, y, 24).x, y, t(settings_, "bind_menu"), ui::rgba(0.75f, 0.76f, 0.72f), 0.62f);
        y += 36;
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) {
            save_settings(settings_);
            page_ = Page::Settings;
        }
    } else if (page_ == Page::Sound) {
        ui.slider(ui::hash_id("master"), row(56), &settings_.master, 0.0f, 1.0f, t(settings_, "master"));
        ui.slider(ui::hash_id("music"), row(56), &settings_.music, 0.0f, 1.0f, t(settings_, "music"));
        ui.slider(ui::hash_id("sfx"), row(56), &settings_.sfx, 0.0f, 1.0f, t(settings_, "sfx"));
        const auto note = center_column(ui, y, 80);
        ui.text(note.x, y, t(settings_, "audio_note"), ui::rgba(0.65f, 0.66f, 0.62f), 0.58f);
        y += 90;
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) {
            save_settings(settings_);
            page_ = Page::Settings;
        }
    } else if (page_ == Page::Language) {
        bool ru = settings_.language != "en";
        bool en = settings_.language == "en";
        if (ui.checkbox(ui::hash_id("ru"), row(40), &ru, "Русский") && ru) settings_.language = "ru";
        if (ui.checkbox(ui::hash_id("en"), row(40), &en, "English") && en) settings_.language = "en";
        if (ui.button(ui::hash_id("back"), row(), t(settings_, "back"))) {
            save_settings(settings_);
            page_ = Page::Settings;
        }
    }
    return result;
}

namespace {

void stat_bar(ui::Ui& ui, float x, float y, float w, const char* label, float frac, ui::Color color)
{
    ui.text(x, y + 5.0f, label, color, 0.42f);
    const float bx = x + 108.0f;
    const float bw = w - 108.0f;
    const float bh = 14.0f;
    const float by = y + 5.0f;
    ui::BoxStyle bg;
    bg.fill = ui::rgba(0.11f, 0.14f, 0.16f, 0.95f);
    bg.border = ui::rgba(0.29f, 0.36f, 0.40f);
    bg.border_px = 1.0f;
    ui.box({bx, by, bw, bh}, bg);
    const float fw = (bw - 4.0f) * std::clamp(frac, 0.0f, 1.0f);
    if (fw > 0.5f) ui.quad({bx + 2.0f, by + 2.0f, fw, bh - 4.0f}, color);
}

void corner_ticks(ui::Ui& ui, float x, float y, float w, float h, ui::Color color)
{
    const float arm = 8.0f;
    const float t = 2.0f;
    ui.quad({x - 1, y - 1, arm, t}, color);
    ui.quad({x - 1, y - 1, t, arm}, color);
    ui.quad({x + w - arm + 1, y - 1, arm, t}, color);
    ui.quad({x + w - t + 1, y - 1, t, arm}, color);
    ui.quad({x - 1, y + h - t + 1, arm, t}, color);
    ui.quad({x - 1, y + h - arm + 1, t, arm}, color);
    ui.quad({x + w - arm + 1, y + h - t + 1, arm, t}, color);
    ui.quad({x + w - t + 1, y + h - arm + 1, t, arm}, color);
}

} // namespace

void draw_game_hud(ui::Ui& ui, const render::DebugState& debug)
{
    char line[160];
    constexpr float panel_w = 320.0f;
    constexpr float panel_h = 156.0f;
    const float px = 12.0f;
    const float py = static_cast<float>(ui.height()) - panel_h - 12.0f;
    ui::BoxStyle glass;
    glass.fill = ui::rgba(0.07f, 0.09f, 0.11f, 0.60f);
    glass.radius = 3.0f;
    glass.border = ui::rgba(0.29f, 0.36f, 0.40f);
    glass.border_px = 1.0f;
    glass.backdrop_px = 12.0f;
    ui.box({px, py, panel_w, panel_h}, glass);
    corner_ticks(ui, px, py, panel_w, panel_h, ui::rgba(0.55f, 0.64f, 0.69f));

    const float x = px + 8.0f;
    const float w = panel_w - 16.0f;
    float y = py + 8.0f;
    const ui::Color o2{0.36f, 0.78f, 0.91f, 1.0f};
    const ui::Color stam{0.91f, 0.58f, 0.29f, 1.0f};
    const ui::Color rad{0.60f, 0.80f, 0.20f, 1.0f};
    const ui::Color temp{0.35f, 0.63f, 0.90f, 1.0f};
    const ui::Color hp{0.90f, 0.30f, 0.32f, 1.0f};
    std::snprintf(line, sizeof(line), "OXYGEN %.0f%%", debug.o2);
    stat_bar(ui, x, y, w, line, debug.o2 / 100.0f, o2);
    y += 28.0f;
    std::snprintf(line, sizeof(line), "STAMINA %.0f%%", debug.stamina);
    stat_bar(ui, x, y, w, line, debug.stamina / 100.0f, stam);
    y += 28.0f;
    std::snprintf(line, sizeof(line), "RADIATION %.0f%%", debug.radiation);
    stat_bar(ui, x, y, w, line, debug.radiation / 100.0f, rad);
    y += 28.0f;
    // Body-temperature readout derived from cold exposure: 36.6 healthy .. 20.0 freezing.
    const float temp_c = 36.6f - debug.cold * 0.166f;
    std::snprintf(line, sizeof(line), "TEMPERATURE %.1f", temp_c);
    stat_bar(ui, x, y, w, line, 1.0f - debug.cold / 100.0f, temp);
    y += 28.0f;
    std::snprintf(line, sizeof(line), "HEALTH %.0f%%", debug.hp);
    stat_bar(ui, x, y, w, line, debug.hp / 100.0f, hp);
}

void draw_world_captions(ui::Ui& ui, const Camera& camera, const render::DebugState& debug, int width, int height)
{
    if (debug.world_label_count == 0) return;
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 clip_from_world = camera.projection(aspect) * camera.view();
    for (std::uint32_t i = 0; i < debug.world_label_count; ++i) {
        const char* text = debug.world_label_text[i];
        if (!text || !text[0]) continue;
        const glm::vec4 clip = clip_from_world * glm::vec4(debug.world_label_pos[i], 1.0f);
        if (clip.w <= 0.15f) continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < 0.0f || ndc.z > 1.0f) continue;
        const float x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
        const float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);
        ui.text_center(x, y - 18.0f, text, ui::rgba(0.95f, 0.95f, 0.9f), 0.55f);
    }
}

} // namespace forge::app
