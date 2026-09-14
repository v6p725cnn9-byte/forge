#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace forge::game {

enum class Item : std::uint8_t {
    None, Wood, Stone, Stick, Flint, Fiber, Rope, StoneAxe, StonePickaxe, Campfire,
    IronOre, IronIngot, Furnace, Bench, IronAxe, IronPickaxe, Bandage, Count
};
constexpr std::size_t kItemCount = static_cast<std::size_t>(Item::Count);
constexpr std::size_t index(Item item) { return static_cast<std::size_t>(item); }
constexpr bool valid(Item item) { return index(item) < kItemCount; }
enum class Tool : std::uint8_t { None, Axe, Pickaxe };
struct ItemDef {
    const char* ru;
    const char* en;
    float kg;
    std::uint16_t stack;
    Tool tool = Tool::None;
    std::uint16_t durability = 0;
};
inline constexpr std::array<ItemDef, kItemCount> kItems{{
    {"Руки", "Hands", 0, 0}, {"Древесина", "Wood", .4f, 100},
    {"Камень", "Stone", .5f, 100}, {"Ветки", "Sticks", .1f, 100},
    {"Кремень", "Flint", .2f, 100}, {"Волокно", "Fiber", .02f, 200},
    {"Верёвка", "Rope", .1f, 50}, {"Каменный топор", "Stone axe", 1.5f, 1, Tool::Axe, 40},
    {"Каменная кирка", "Stone pickaxe", 2, 1, Tool::Pickaxe, 40},
    {"Костёр", "Campfire", 3, 5}, {"Железная руда", "Iron ore", .6f, 100},
    {"Железный слиток", "Iron ingot", .4f, 50}, {"Печь", "Furnace", 8, 1},
    {"Верстак", "Workbench", 5, 1}, {"Железный топор", "Iron axe", 1.8f, 1, Tool::Axe, 100},
    {"Железная кирка", "Iron pickaxe", 2.2f, 1, Tool::Pickaxe, 100},
    {"Повязка", "Bandage", .1f, 20}
}};
inline const ItemDef& definition(Item item) { return kItems[valid(item) ? index(item) : 0]; }
inline const char* item_name(Item item, std::string_view language) {
    return language == "en" ? definition(item).en : definition(item).ru;
}
struct Inventory {
    static constexpr float kMaxKg = 45;
    std::array<std::uint16_t, kItemCount> count{};
    std::array<std::uint16_t, kItemCount> durability{};
    Item equipped = Item::None;
    std::uint16_t& operator[](Item item) { return count[index(item)]; }
    std::uint16_t operator[](Item item) const { return count[index(item)]; }
    float weight() const {
        float kg = 0;
        for (std::size_t i = 1; i < kItemCount; ++i) kg += count[i] * kItems[i].kg;
        return kg;
    }
    bool add(Item item, std::uint16_t n) {
        if (!valid(item) || item == Item::None || n > definition(item).stack - count[index(item)]
            || weight() + n * definition(item).kg > kMaxKg + .001f) return false;
        count[index(item)] += n;
        if (definition(item).durability) durability[index(item)] = definition(item).durability;
        return true;
    }
};
enum class Station : std::uint8_t { Hand, Furnace, Bench };
struct Ingredient { Item item = Item::None; std::uint16_t count = 0; };
struct Recipe { Item output; std::uint16_t count; std::array<Ingredient, 3> cost; Station station = Station::Hand; };
inline constexpr std::array<Recipe, 11> kRecipes{{
    {Item::Rope, 1, {{{Item::Fiber, 4}}}},
    {Item::StoneAxe, 1, {{{Item::Stick, 4}, {Item::Stone, 4}, {Item::Rope, 1}}}},
    {Item::StonePickaxe, 1, {{{Item::Stick, 4}, {Item::Flint, 4}, {Item::Rope, 2}}}},
    {Item::Campfire, 1, {{{Item::Wood, 6}, {Item::Stone, 8}}}},
    {Item::Stick, 4, {{{Item::Wood, 1}}}},
    {Item::Bandage, 1, {{{Item::Fiber, 8}}}},
    {Item::Furnace, 1, {{{Item::Stone, 20}, {Item::Wood, 10}, {Item::Rope, 2}}}},
    {Item::Bench, 1, {{{Item::Wood, 12}, {Item::Stone, 6}, {Item::Rope, 4}}}},
    {Item::IronIngot, 1, {{{Item::IronOre, 2}, {Item::Wood, 1}}}, Station::Furnace},
    {Item::IronAxe, 1, {{{Item::IronIngot, 4}, {Item::Stick, 4}, {Item::Rope, 2}}}, Station::Bench},
    {Item::IronPickaxe, 1, {{{Item::IronIngot, 6}, {Item::Stick, 4}, {Item::Rope, 2}}}, Station::Bench}
}};
enum class Action : std::uint8_t { None, Craft, Equip, Repair, Use, Discard };
enum class Result : std::uint8_t { None, Ok, Missing, Full, NeedAxe, NeedPickaxe, Broken, TooFar, NeedStation, Invalid, Cooldown };
inline Result craft_inventory(Inventory& inventory, std::uint8_t recipe_id) {
    if (recipe_id >= kRecipes.size()) return Result::Invalid;
    const auto& recipe = kRecipes[recipe_id];
    auto next = inventory;
    for (const auto& cost : recipe.cost) {
        if (next[cost.item] < cost.count) return Result::Missing;
        next[cost.item] -= cost.count;
    }
    if (!next.add(recipe.output, recipe.count)) return Result::Full;
    if (definition(recipe.output).tool != Tool::None) next.equipped = recipe.output;
    inventory = next;
    return Result::Ok;
}
inline const char* result_text(Result result, bool en) {
    switch (result) {
    case Result::Ok: return en ? "Done" : "Готово";
    case Result::Missing: return en ? "Not enough materials" : "Не хватает материалов";
    case Result::Full: return en ? "Backpack full / stack limit" : "Рюкзак заполнен / лимит стопки";
    case Result::NeedAxe: return en ? "Equip an axe to chop trees" : "Для дерева нужен топор в руках";
    case Result::NeedPickaxe: return en ? "Equip a pickaxe to mine" : "Для валуна и руды нужна кирка";
    case Result::Broken: return en ? "Tool broken — repair in inventory" : "Инструмент сломан — почините в инвентаре";
    case Result::TooFar: return en ? "Move closer to a resource" : "Подойдите ближе к ресурсу";
    case Result::NeedStation: return en ? "Required station must be within 4 m" : "Нужная станция должна быть в 4 м";
    case Result::Invalid: return en ? "Action unavailable" : "Действие недоступно";
    case Result::Cooldown: return en ? "Wait for the next strike" : "Дождитесь следующего удара";
    default: return "";
    }
}
} // namespace forge::game
