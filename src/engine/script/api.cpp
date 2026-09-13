#include "engine/script/vm.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace forge::script {
namespace {

Registry* world(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "forge.world");
    auto* registry = static_cast<Registry*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return registry;
}

int fail(lua_State* L, const char* message)
{
    lua_pushnil(L);
    lua_pushstring(L, message);
    return 2;
}

int spawn_player(lua_State* L)
{
    const auto x = static_cast<float>(luaL_checknumber(L, 1));
    const auto y = static_cast<float>(luaL_checknumber(L, 2));
    const auto z = static_cast<float>(luaL_checknumber(L, 3));
    const int id = world(L)->spawn_player({x, y, z});
    if (id < 0) return fail(L, "player slots full");
    lua_pushinteger(L, id);
    return 1;
}

int spawn_vehicle(lua_State* L)
{
    const auto x = static_cast<float>(luaL_checknumber(L, 1));
    const auto y = static_cast<float>(luaL_checknumber(L, 2));
    const auto z = static_cast<float>(luaL_checknumber(L, 3));
    const auto yaw = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    const int id = world(L)->spawn_vehicle({x, y, z}, yaw);
    if (id < 0) return fail(L, "vehicle slots full");
    lua_pushinteger(L, id);
    return 1;
}

int create_marker(lua_State* L)
{
    const auto x = static_cast<float>(luaL_checknumber(L, 1));
    const auto y = static_cast<float>(luaL_checknumber(L, 2));
    const auto z = static_cast<float>(luaL_checknumber(L, 3));
    const auto size = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    const auto r = static_cast<float>(luaL_optnumber(L, 5, 0.2));
    const auto g = static_cast<float>(luaL_optnumber(L, 6, 0.8));
    const auto b = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    const int id = world(L)->create_marker({x, y, z}, size, {r, g, b, 1.0f});
    if (id < 0) return fail(L, "marker slots full");
    lua_pushinteger(L, id);
    return 1;
}

int create_label(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    const auto x = static_cast<float>(luaL_checknumber(L, 2));
    const auto y = static_cast<float>(luaL_checknumber(L, 3));
    const auto z = static_cast<float>(luaL_checknumber(L, 4));
    const auto distance = static_cast<float>(luaL_optnumber(L, 5, 25.0));
    const int id = world(L)->create_label(text, {x, y, z}, distance);
    if (id < 0) return fail(L, "label slots full");
    lua_pushinteger(L, id);
    return 1;
}

int set_player_name(lua_State* L)
{
    auto* player = world(L)->player(static_cast<int>(luaL_checkinteger(L, 1)));
    if (!player) return fail(L, "invalid player");
    player->name = luaL_checkstring(L, 2);
    lua_pushboolean(L, 1);
    return 1;
}

int get_player_position(lua_State* L)
{
    const auto* player = world(L)->player(static_cast<int>(luaL_checkinteger(L, 1)));
    if (!player) return fail(L, "invalid player");
    lua_pushnumber(L, player->position.x);
    lua_pushnumber(L, player->position.y);
    lua_pushnumber(L, player->position.z);
    return 3;
}

int set_player_position(lua_State* L)
{
    auto* player = world(L)->player(static_cast<int>(luaL_checkinteger(L, 1)));
    if (!player) return fail(L, "invalid player");
    player->position = {static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                        static_cast<float>(luaL_checknumber(L, 4))};
    lua_pushboolean(L, 1);
    return 1;
}

int destroy_vehicle(lua_State* L)
{
    lua_pushboolean(L, world(L)->destroy_vehicle(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

int destroy_marker(lua_State* L)
{
    lua_pushboolean(L, world(L)->destroy_marker(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

int destroy_label(lua_State* L)
{
    lua_pushboolean(L, world(L)->destroy_label(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

const luaL_Reg natives[] = {
    {"SpawnPlayer", spawn_player},
    {"SpawnVehicle", spawn_vehicle},
    {"CreateMarker", create_marker},
    {"Create3DTextLabel", create_label},
    {"SetPlayerName", set_player_name},
    {"GetPlayerPos", get_player_position},
    {"SetPlayerPos", set_player_position},
    {"DestroyVehicle", destroy_vehicle},
    {"DestroyMarker", destroy_marker},
    {"Destroy3DTextLabel", destroy_label},
    {nullptr, nullptr},
};

} // namespace

void register_api(lua_State* state, Registry& registry)
{
    lua_pushlightuserdata(state, &registry);
    lua_setfield(state, LUA_REGISTRYINDEX, "forge.world");
    lua_pushglobaltable(state);
    luaL_setfuncs(state, natives, 0);
    lua_pop(state, 1);
}

} // namespace forge::script
