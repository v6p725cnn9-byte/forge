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
    ui.text_center(static_cast<float>(ui.width()) * 0.5f, 70.0f, title, ui::rgba(0.92f, 0.88f, 0.55f), 1.85f);
    ui.text_center(static_cast<float>(ui.width()) * 0.5f, 140.0f, t(settings_, "subtitle"), ui::rgba(0.65f, 0.68f, 0.62f),
                   0.75f);

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

void draw_game_hud(ui::Ui& ui, const render::DebugState& debug)
{
    const char* lang = debug.language ? debug.language : "ru";
    char line[160];
    const float x = 24;
    float y = static_cast<float>(ui.height()) - 150;
    ui.quad({12, y - 12, 520, 140}, ui::rgba(0.05f, 0.07f, 0.06f, 0.72f));
    std::snprintf(line, sizeof(line), "%s %.0f    %s %.0f", tr(lang, "hp"), debug.hp, tr(lang, "cold"), debug.cold);
    ui.text(x, y, line, ui::rgba(0.92f, 0.93f, 0.9f), 0.62f);
    y += 32;
    std::snprintf(line, sizeof(line), "%s %u    %s %u", tr(lang, "wood"), debug.wood, tr(lang, "stone"), debug.stone);
    ui.text(x, y, line, ui::rgba(0.92f, 0.93f, 0.9f), 0.62f);
    y += 32;
    const char* phase = debug.phase == 1 ? "hud_extracted" : (debug.phase == 2 ? "hud_failed" : "hud_drop");
    std::snprintf(line, sizeof(line), "%s  %u s  |  %s  |  %s", tr(lang, "hud_session"), debug.time_left,
                  debug.night ? tr(lang, "hud_night") : tr(lang, "hud_day"), tr(lang, phase));
    ui.text(x, y, line, ui::rgba(0.85f, 0.86f, 0.8f), 0.58f);
    y += 30;
    if (debug.join_hint[0] && debug.net_role && std::string_view(debug.net_role) == "host")
        ui.text(x, y, debug.join_hint, ui::rgba(0.7f, 0.85f, 0.7f), 0.52f);
    else
        ui.text(x, y, tr(lang, "hud_help"), ui::rgba(0.6f, 0.62f, 0.58f), 0.5f);
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
