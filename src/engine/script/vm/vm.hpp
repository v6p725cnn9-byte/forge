#pragma once

#include "engine/script/bindings/registry.hpp"

#include <filesystem>
#include <string>
#include <string_view>

struct lua_State;

namespace forge {
class Camera;
}

namespace forge::script {

class Vm {
public:
    Vm() = default;
    ~Vm() { close(); }
    Vm(const Vm&) = delete;
    Vm& operator=(const Vm&) = delete;

    // When a camera is bound, the gamemode gains a `camera` table:
    // camera.setFirstPerson(), camera.setThirdPerson() and
    // camera.getCurrentPerson() ("firstPerson" or "thirdPerson").
    // The table mutates the live camera, so Lua and C++ always agree.
    bool open(Registry& world, Camera* camera = nullptr);
    void close();
    bool run_file(const std::filesystem::path& path);
    bool run_string(std::string_view source, const char* name = "chunk");
    bool call(const char* function);
    bool call(const char* function, float dt);
    const std::string& last_error() const { return error_; }
    bool ready() const { return state_ != nullptr; }

private:
    bool pcall(int args);
    lua_State* state_ = nullptr;
    std::string error_;
};

void register_api(lua_State* state, Registry& world, Camera* camera = nullptr);

} // namespace forge::script
