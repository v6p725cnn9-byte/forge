#include "engine/net/client.hpp"
#include "engine/net/interest.hpp"
#include "engine/net/protocol.hpp"
#include "engine/net/server.hpp"
#include "engine/script/registry.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

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
    using namespace forge::net;

    Snapshot original;
    original.tick = 42;
    original.self = 3;
    original.ack = 9;
    Ghost player;
    player.kind = Kind::Player;
    player.id = 3;
    player.position = {1.25f, 2.0f, -4.5f};
    player.yaw = 90.0f;
    player.name = "CJ";
    Ghost marker;
    marker.kind = Kind::Marker;
    marker.id = 1;
    marker.position = {8, 0.2f, 3};
    marker.size = 1.5f;
    marker.color = {0.25f, 0.5f, 1.0f, 1.0f};
    Ghost label;
    label.kind = Kind::Label;
    label.id = 0;
    label.position = {0, 2, 0};
    label.text = "Hello";
    original.entities = {player, marker, label};
    const auto packed = pack_snapshot(original);
    Snapshot decoded;
    check(unpack_snapshot(packed.data(), packed.size(), decoded), "snapshot unpack");
    check(decoded.tick == 42 && decoded.self == 3 && decoded.ack == 9, "snapshot header");
    check(decoded.entities.size() == 3 && decoded.entities[0].name == "CJ", "player name");
    check(decoded.entities[2].text == "Hello", "label text");
    check(std::abs(decoded.entities[1].size - 1.5f) < 1e-5f, "marker size");

    Input input{7, 0.5f, -1.0f, 45.0f, true};
    const auto input_bytes = pack_input(input);
    Input input_out{};
    check(unpack_input(input_bytes.data(), input_bytes.size(), input_out), "input unpack");
    check(input_out.seq == 7 && input_out.boost && std::abs(input_out.yaw - 45.0f) < 1e-4f, "input fields");

    forge::script::Registry world;
    world.spawn_player({0, 1, 0});
    world.spawn_vehicle({10, 1, 0}, 0);
    world.spawn_vehicle({80, 1, 0}, 0);
    world.create_marker({0, 0.2f, 0}, 1, {1, 1, 1, 1});
    world.create_label("Far", {0, 2, 90}, 20);
    const auto near = collect_stream(world, {0, 1, 0}, 50.0f, 0);
    bool saw_near_car = false, saw_far_car = false, saw_far_label = false;
    for (const auto& ghost : near) {
        if (ghost.kind == Kind::Vehicle && ghost.id == 0) saw_near_car = true;
        if (ghost.kind == Kind::Vehicle && ghost.id == 1) saw_far_car = true;
        if (ghost.kind == Kind::Label) saw_far_label = true;
    }
    check(saw_near_car && !saw_far_car, "stream radius must hide the far vehicle");
    check(!saw_far_label, "label draw distance must hide the far text");
    check(xz_distance({0, 0, 0}, {3, 9, 4}) == 5.0f, "xz distance ignores Y");

    Server server;
    Client client;
    check(server.listen(0), "bind ephemeral");
    server.attach(world);
    check(client.connect(localhost(server.port())), "connect loopback");
    bool ok = false;
    for (int i = 0; i < 40; ++i) {
        client.send_input(0, 0, 0, false);
        server.update(0.05f);
        client.poll();
        if (client.connected() && !client.snapshot().entities.empty()) {
            ok = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(ok, "loopback handshake must produce a snapshot");
    check(client.player_id() < 32, "assigned player id");
    bool saw_self = false;
    for (const auto& ghost : client.snapshot().entities)
        if (ghost.kind == Kind::Player && ghost.id == client.player_id()) saw_self = true;
    check(saw_self, "snapshot must include the local player");

    std::cout << "Net checks passed (port " << server.port() << ", player " << static_cast<int>(client.player_id())
              << ")\n";
}
