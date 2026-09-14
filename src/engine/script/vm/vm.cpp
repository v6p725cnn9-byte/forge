#include "engine/script/vm/vm.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace forge::script {
namespace {

void open_sandbox(lua_State* L)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
    lua_pushnil(L);
    lua_setglobal(L, "load");
    lua_pushnil(L);
    lua_setglobal(L, "loadstring");
}

} // namespace

bool Vm::open(game::World& world, Camera* camera)
{
    close();
    state_ = luaL_newstate();
    if (!state_) {
        error_ = "luaL_newstate failed";
        return false;
    }
    open_sandbox(state_);
    register_api(state_, world, camera);
    error_.clear();
    return true;
}

void Vm::close()
{
    if (!state_) return;
    lua_close(state_);
    state_ = nullptr;
}

bool Vm::pcall(int args)
{
    const int status = lua_pcall(state_, args, 0, 0);
    if (status == LUA_OK) {
        error_.clear();
        return true;
    }
    error_ = lua_tostring(state_, -1) ? lua_tostring(state_, -1) : "lua error";
    lua_pop(state_, 1);
    return false;
}

bool Vm::run_string(std::string_view source, const char* name)
{
    if (!state_) {
        error_ = "VM is closed";
        return false;
    }
    if (luaL_loadbuffer(state_, source.data(), source.size(), name) != LUA_OK) {
        error_ = lua_tostring(state_, -1) ? lua_tostring(state_, -1) : "parse error";
        lua_pop(state_, 1);
        return false;
    }
    return pcall(0);
}

bool Vm::run_file(const std::filesystem::path& path)
{
    if (!state_) {
        error_ = "VM is closed";
        return false;
    }
    if (luaL_loadfile(state_, path.string().c_str()) != LUA_OK) {
        error_ = lua_tostring(state_, -1) ? lua_tostring(state_, -1) : "cannot load file";
        lua_pop(state_, 1);
        return false;
    }
    return pcall(0);
}

bool Vm::call(const char* function)
{
    if (!state_) {
        error_ = "VM is closed";
        return false;
    }
    lua_getglobal(state_, function);
    if (lua_isnil(state_, -1)) {
        lua_pop(state_, 1);
        error_.clear();
        return true;
    }
    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 1);
        error_ = std::string(function) + " is not a function";
        return false;
    }
    return pcall(0);
}

bool Vm::call(const char* function, float dt)
{
    if (!state_) {
        error_ = "VM is closed";
        return false;
    }
    lua_getglobal(state_, function);
    if (lua_isnil(state_, -1)) {
        lua_pop(state_, 1);
        error_.clear();
        return true;
    }
    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 1);
        error_ = std::string(function) + " is not a function";
        return false;
    }
    lua_pushnumber(state_, dt);
    return pcall(1);
}

} // namespace forge::script
