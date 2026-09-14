#include "engine/frontend/inventory/inventory_menu.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

namespace forge::app {
InventoryCommand InventoryMenu::draw(ui::Ui& ui, const net::Snapshot& snapshot, bool pending, const char* language)
{
    using namespace game;
    const bool en = std::string_view(language) == "en";
    const auto tr = [en](const char* ru, const char* eng) { return en ? eng : ru; };
    const auto& bag = snapshot.inventory;
    InventoryCommand command;
    const float w = std::min(1120.0f, ui.width() - 32.0f), h = 630;
    const float x = (ui.width() - w) * .5f, y = (ui.height() - h) * .5f;
    const ui::Color white{.90f,.94f,.92f,1}, muted{.56f,.66f,.64f,1}, accent{.55f,.84f,.66f,1};
    ui.quad({0,0,static_cast<float>(ui.width()),static_cast<float>(ui.height())},{.01f,.025f,.025f,.65f});
    ui::BoxStyle panel;
    panel.fill = {.025f,.065f,.065f,.96f}; panel.border = {.25f,.42f,.39f,.8f};
    panel.radius = 14; panel.border_px = 1; panel.backdrop_px = 8;
    ui.box({x,y,w,h},panel);
    ui.text(x+24,y+18,tr("СНАРЯЖЕНИЕ", "EQUIPMENT"),accent,.95f);
    ui.text(x+24,y+55,tr("Tab / I - закрыть | мир продолжает жить", "Tab / I - close | the world keeps running"),muted,.48f);
    if (ui.button(8100,{x+w-122,y+18,98,36},tr("Закрыть","Close"))) command.close = true;
    const char* tabs[]{tr("Инвентарь","Inventory"),tr("Крафты","Crafting")};
    ui.tabs(8101,{x+24,y+88,w-48,42},tabs,2,&tab_);
    const float left_w = (w-72)*.60f, right_x = x+48+left_w, right_w = w-72-left_w;
    const float top = y+150;
    Item detail = selected_;
    if (tab_ == 0) {
        const float cell_w = (left_w-18)/3;
        for (std::size_t i = 1; i < kItemCount; ++i) {
            const auto item = static_cast<Item>(i);
            const float cx = x+24+((i-1)%3)*(cell_w+9), cy = top+((i-1)/3)*62;
            const auto count = bag[item];
            std::string label = item_name(item,language);
            label += "  x" + std::to_string(count);
            if (ui.button(8200+static_cast<std::uint32_t>(i),{cx,cy,cell_w,54},label)) selected_ = item;
            if (item == selected_) ui.quad({cx,cy,3,54},accent);
            if (!count) ui.quad({cx,cy,cell_w,54},{.015f,.025f,.025f,.45f});
            if (definition(item).durability && count)
                ui.progress({cx+8,cy+46,cell_w-16,3},static_cast<float>(bag.durability[i])/definition(item).durability);
        }
        detail = selected_;
    } else {
        const float row_w = (left_w-10)/2;
        for (std::size_t i = 0; i < kRecipes.size(); ++i) {
            const float cx = x+24+(i%2)*(row_w+10), cy = top+(i/2)*62;
            const auto& recipe = kRecipes[i];
            auto trial = bag;
            const bool station = recipe.station == Station::Hand
                || (snapshot.stations & (recipe.station == Station::Furnace ? 1 : 2));
            const bool ready = station && craft_inventory(trial,static_cast<std::uint8_t>(i)) == Result::Ok;
            if (ui.button(8300+static_cast<std::uint32_t>(i),{cx,cy,row_w,54},item_name(recipe.output,language)))
                recipe_ = static_cast<int>(i);
            ui.quad({cx,cy,3,54},ready ? accent : muted);
            if (static_cast<int>(i) == recipe_) ui.quad({cx,cy+52,row_w,2},accent);
        }
        detail = kRecipes[recipe_].output;
    }
    const auto& def = definition(detail);
    ui.text(right_x,top,item_name(detail,language),white,.77f);
    char text[120];
    std::snprintf(text,sizeof(text),"%.2f %s / %s",def.kg,tr("кг","kg"),tr("шт.","item"));
    ui.text(right_x,top+35,text,muted,.5f);
    float row = top+75;
    const auto line = [&](const std::string& value, ui::Color color = ui::Color{.75f,.82f,.79f,1}) {
        ui.text(right_x,row,value,color,.55f); row += 31;
    };
    const auto action_button = [&](Action action, std::uint8_t argument, const char* label, bool enabled = true) {
        const ui::Rect r{right_x,row,right_w,40}; row += 49;
        if (enabled && !pending) {
            if (ui.button(8400+static_cast<std::uint32_t>(action),r,label)) command = {action,argument,false};
        } else {
            ui.quad(r,{.06f,.10f,.10f,1});
            ui.text(r.x+10,r.y+10,label,muted,.55f);
        }
    };
    if (tab_ == 1) {
        const auto& recipe = kRecipes[recipe_];
        line(tr("МАТЕРИАЛЫ", "MATERIALS"));
        for (const auto& cost : recipe.cost) {
            if (!cost.count) continue;
            line(std::string(item_name(cost.item,language))+"  "+std::to_string(bag[cost.item])+" / "+std::to_string(cost.count),
                 bag[cost.item] >= cost.count ? accent : ui::Color{.96f,.56f,.4f,1});
        }
        line(recipe.station == Station::Hand ? tr("Создание вручную","Craft by hand")
            : recipe.station == Station::Furnace ? tr("Нужна печь рядом","Needs nearby furnace") : tr("Нужен верстак рядом","Needs nearby workbench"));
        line(std::string(tr("Выход: ","Output: "))+std::to_string(recipe.count));
        auto trial = bag;
        const bool station = recipe.station == Station::Hand || (snapshot.stations & (recipe.station == Station::Furnace ? 1 : 2));
        action_button(Action::Craft,static_cast<std::uint8_t>(recipe_),tr("Создать","Craft"),
                      station && craft_inventory(trial,static_cast<std::uint8_t>(recipe_)) == Result::Ok);
    } else {
        line(std::string(tr("В рюкзаке: ","In backpack: "))+std::to_string(bag[detail]));
        if (def.durability) {
            line(std::string(tr("Прочность: ","Durability: "))+std::to_string(bag.durability[index(detail)])+" / "+std::to_string(def.durability));
            line(def.tool == Tool::Axe ? tr("Добывает древесину","Harvests wood") : tr("Добывает камень и руду","Mines stone and ore"));
            const bool equipped = bag.equipped == detail;
            action_button(Action::Equip,static_cast<std::uint8_t>(equipped ? Item::None : detail),
                          equipped ? tr("Убрать в рюкзак","Unequip") : tr("Взять в руки","Equip"),bag[detail] > 0);
            const bool iron = detail == Item::IronAxe || detail == Item::IronPickaxe;
            line(iron ? tr("Ремонт: 2 слитка + 1 верёвка","Repair: 2 ingots + 1 rope")
                      : tr("Ремонт: 2 кремня + 1 верёвка","Repair: 2 flint + 1 rope"));
            action_button(Action::Repair,static_cast<std::uint8_t>(detail),tr("Починить","Repair"),
                          bag[detail] && bag.durability[index(detail)] < def.durability);
        } else if (detail == Item::Campfire || detail == Item::Bench || detail == Item::Furnace || detail == Item::Bandage) {
            line(detail == Item::Bandage ? tr("Восстанавливает 25 здоровья","Restores 25 health") : tr("Размещается перед персонажем","Places in front of your character"));
            action_button(Action::Use,static_cast<std::uint8_t>(detail),detail == Item::Bandage ? tr("Применить","Use") : tr("Разместить","Place"),bag[detail] > 0);
        } else {
            line(tr("Материал для крафта","Crafting material"));
        }
        action_button(Action::Discard,static_cast<std::uint8_t>(detail),tr("Уничтожить 1 шт.","Destroy 1 item"),bag[detail] > 0);
    }
    std::snprintf(text,sizeof(text),"%.1f / %.0f %s",bag.weight(),Inventory::kMaxKg,tr("кг","kg"));
    ui.text(x+24,y+h-70,text,white,.6f);
    ui.progress({x+24,y+h-38,left_w,6},bag.weight()/Inventory::kMaxKg);
    ui.text(right_x,y+h-67,pending ? tr("Ожидание сервера…","Waiting for server…") : result_text(snapshot.feedback,en),accent,.48f);
    ui.text(right_x,y+h-37,tr("E - собрать / ударить | C - костёр","E - gather / strike | C - campfire"),muted,.43f);
    return command;
}
} // namespace forge::app
