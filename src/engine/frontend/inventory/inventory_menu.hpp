#pragma once
#include "engine/game/inventory/items.hpp"
#include "engine/game_net/snapshots/packets.hpp"
#include "engine/ui/widgets/ui.hpp"

namespace forge::app {
struct InventoryCommand { game::Action action = game::Action::None; std::uint8_t argument = 0; bool close = false; };
class InventoryMenu {
public:
    void reset(int tab = 0) { tab_ = tab; selected_ = game::Item::Stick; recipe_ = 0; }
    InventoryCommand draw(ui::Ui& ui, const net::Snapshot& snapshot, bool pending, const char* language);
private:
    int tab_ = 0;
    game::Item selected_ = game::Item::Stick;
    int recipe_ = 0;
};
} // namespace forge::app
