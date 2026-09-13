#include "engine/script/vm.hpp"

#include <cmath>
#include <cstdlib>
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
    forge::script::Registry world;
    forge::script::Vm vm;
    check(vm.open(world), vm.last_error().c_str());
    check(vm.run_string("id = SpawnPlayer(1.5, 2.0, 3.5)\n"
                        "SetPlayerName(id, 'CJ')\n"
                        "vid = SpawnVehicle(4, 1, 8, 90)\n"
                        "mid = CreateMarker(0, 0.2, 10, 1.5, 0.1, 0.9, 1.0)\n"
                        "lid = Create3DTextLabel('Hello', 0, 2, 0, 20)\n",
                        "boot"),
          vm.last_error().c_str());
    check(world.alive_players() == 1 && world.player(0) && world.player(0)->name == "CJ", "SpawnPlayer");
    check(std::abs(world.player(0)->position.x - 1.5f) < 1e-5f, "player x");
    check(world.alive_vehicles() == 1 && std::abs(world.vehicle(0)->yaw - 90.0f) < 1e-4f, "SpawnVehicle heading");
    check(world.alive_markers() == 1 && world.marker(0)->size > 1.0f, "CreateMarker");
    check(world.alive_labels() == 1 && world.labels()[0].text == "Hello", "Create3DTextLabel");
    check(vm.run_string("x,y,z = GetPlayerPos(0)\nassert(math.abs(x-1.5)<0.001)\nSetPlayerPos(0, 9, 1, 2)\n",
                        "move"),
          vm.last_error().c_str());
    check(std::abs(world.player(0)->position.x - 9.0f) < 1e-5f, "SetPlayerPos");
    check(vm.call("missing_callback"), "missing callback must be a no-op");

    check(vm.run_string("function on_init() SpawnPlayer(0,1,0) end", "initfn"), vm.last_error().c_str());
    check(vm.call("on_init"), vm.last_error().c_str());
    check(world.alive_players() == 2, "on_init spawn");

    check(!vm.run_string("os.execute('true')", "sandbox-os"), "os must be unavailable");
    check(vm.last_error().find("os") != std::string::npos || vm.last_error().find("nil") != std::string::npos,
          "sandbox error should mention os/nil");
    check(!vm.run_string("dofile('x.lua')", "sandbox-dofile"), "dofile must be removed");
    check(vm.run_string("assert(loadfile == nil and io == nil and debug == nil and package == nil)", "no-libs"),
          vm.last_error().c_str());

    check(vm.run_string("assert(DestroyMarker(0) == true)", "destroy"), vm.last_error().c_str());
    check(world.alive_markers() == 0, "DestroyMarker");
    check(vm.run_string("local ok, err = SpawnPlayer(0,0,0)\n"
                        "for i=1,40 do SpawnPlayer(0,0,0) end\n"
                        "local id, msg = SpawnPlayer(0,0,0)\n"
                        "assert(id == nil)\n",
                        "cap"),
          vm.last_error().c_str());

    std::cout << "Script checks passed\n";
}
