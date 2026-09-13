#pragma once

#include "engine/app/settings.hpp"
#include "engine/core/camera.hpp"
#include "engine/render/settings.hpp"
#include "engine/ui/ui.hpp"

#include <string>

namespace forge::app {

enum class MenuCommand { None, Solo, Host, Join, Quit };

struct MenuResult {
    MenuCommand command = MenuCommand::None;
    std::string join;
};

class Menu {
public:
    explicit Menu(Settings& settings);
    MenuResult draw(ui::Ui& ui);
    void reset();
    bool consume_escape();

private:
    enum class Page { Root, Multiplayer, Settings, Graphics, Controls, Sound, Language };

    Settings& settings_;
    Settings draft_;
    Page page_ = Page::Root;
    char join_[64]{};
    char port_[8]{};
};

void draw_game_hud(ui::Ui& ui, const render::DebugState& debug);
void draw_world_captions(ui::Ui& ui, const Camera& camera, const render::DebugState& debug, int width, int height);

} // namespace forge::app
