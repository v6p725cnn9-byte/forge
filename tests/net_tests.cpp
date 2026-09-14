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
#include <limits>

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
    check(!input_out.jump, "jump defaults to false");
    input.jump = true;
    const auto jump_bytes = pack_input(input);
    check(unpack_input(jump_bytes.data(), jump_bytes.size(), input_out) && input_out.jump, "jump flag round-trips");

    check(sequence_newer(0, 0xffffffffu) && !sequence_newer(0xffffffffu, 0)
          && !sequence_newer(5, 5), "sequence wrap and duplicate ordering");
    Address parsed{};
    check(!parse_address("127.0.0.1:0", parsed), "zero destination port rejected");
    input.move_x = std::numeric_limits<float>::quiet_NaN();
    auto malformed = pack_input(input);
    check(!unpack_input(malformed.data(), malformed.size(), input_out), "NaN movement rejected");
    input.move_x = 1000;
    malformed = pack_input(input);
    check(!unpack_input(malformed.data(), malformed.size(), input_out), "unbounded movement rejected");
    malformed = input_bytes;
    malformed.back() = 128;
    check(!unpack_input(malformed.data(), malformed.size(), input_out), "unknown input flags rejected");
    malformed = pack_hello();
    malformed.push_back(0);
    malformed[6]++;
    check(!unpack_hello(malformed.data(), malformed.size()), "trailing payload rejected");
    Packet type{};
    check(!unpack_type(nullptr, 12, type), "null datagram rejected");
    malformed[6]++;
    check(!unpack_type(malformed.data(), malformed.size(), type), "header length mismatch rejected");
    for (std::size_t n = 0; n < packed.size(); ++n)
        check(!unpack_snapshot(packed.data(), n, decoded), "truncated snapshot rejected");
    Snapshot crowded;
    label.text = std::string(48, 'x');
    crowded.entities.assign(kMaxSnapshotEntities, label);
    const auto budgeted = pack_snapshot(crowded);
    check(budgeted.size() <= kMaxPacket && unpack_snapshot(budgeted.data(), budgeted.size(), decoded),
          "dense labels produce a valid bounded snapshot");
    check(!decoded.entities.empty(), "MTU clipping must retain entities");

    for (const auto& host : ipv4_addresses()) {
        Address local_interface{};
        if (!parse_address(host, local_interface) || local_interface.host == 0) continue;
        Udp lan_receiver;
        local_interface.port = 0;
        if (!lan_receiver.bind(local_interface)) continue;
        local_interface.port = lan_receiver.port();
        Client lan_client;
        check(lan_client.connect(local_interface), "client connects through non-loopback interface");
        Address source{};
        std::uint8_t data[kMaxPacket]{};
        bool routed = false;
        for (int i = 0; i < 100 && !routed; ++i) {
            routed = lan_receiver.receive(source, data, sizeof(data)) > 0;
            if (!routed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(routed && source.host == local_interface.host, "LAN hello uses interface address, not loopback");
        break;
    }

    // A real UDP peer exposes the assigned client endpoint without test-only APIs.
    Udp authority, stranger;
    check(authority.bind(localhost(0)) && stranger.bind(localhost(0)), "fake endpoints bind");
    Udp collision;
    check(!collision.bind(localhost(authority.port())), "live UDP endpoint is exclusive");
    Client guarded;
    check(guarded.connect(localhost(authority.port())), "guarded client connect");
    Address endpoint{};
    std::uint8_t hello[kMaxPacket]{};
    bool received = false;
    for (int i = 0; i < 100 && !received; ++i) {
        received = authority.receive(endpoint, hello, sizeof(hello)) > 0;
        if (!received) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(received, "authority receives client hello");
    auto deliver = [&](Udp& sender, const std::vector<std::uint8_t>& bytes) {
        check(sender.send(endpoint, bytes.data(), bytes.size()), "deliver test datagram");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        guarded.poll();
    };
    const auto welcome = pack_welcome({3, kTickHz, kDefaultStreamRadius});
    deliver(stranger, welcome);
    check(!guarded.connected(), "foreign welcome ignored");
    deliver(authority, packed);
    check(guarded.tick() == 0, "snapshot before handshake ignored");
    deliver(authority, welcome);
    deliver(authority, packed);
    check(guarded.connected() && guarded.tick() == 42, "authoritative snapshot accepted");
    original.tick = 41;
    deliver(authority, pack_snapshot(original));
    check(guarded.tick() == 42, "old snapshot cannot rewind the client");
    original.tick = 43;
    deliver(stranger, pack_snapshot(original));
    check(guarded.tick() == 42, "foreign snapshot ignored");
    original.self = 4;
    deliver(authority, pack_snapshot(original));
    check(guarded.tick() == 42, "wrong player snapshot ignored");

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

    check(collect_stream(world, {0, 1, 0}, 50, 0, -1).empty(), "negative stream cap is empty");
    for (int i = 0; i < 12; ++i) world.spawn_player({0, 1, 0});
    const auto priority = collect_stream(world, {0, 1, 0}, 50, 12, 1);
    check(priority.size() == 1 && priority[0].id == 12, "self survives equal-distance clipping");

    forge::game::Sim sim;
    sim.reset();
    Server server;
    Client client;
    check(server.listen(0), "bind ephemeral");
    server.attach(world);
    server.attach_sim(sim);
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

    Udp raw_client;
    check(raw_client.bind(localhost(0)), "raw client bind");
    const auto server_address = localhost(server.port());
    auto send_raw = [&](const std::vector<std::uint8_t>& bytes) {
        check(raw_client.send(server_address, bytes.data(), bytes.size()), "raw client send");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        server.update(0.05f);
    };
    send_raw(pack_hello());
    Welcome raw_welcome{};
    Address from{};
    const int got = raw_client.receive(from, hello, sizeof(hello));
    check(got > 0 && unpack_welcome(hello, static_cast<std::size_t>(got), raw_welcome), "raw handshake");
    send_raw(pack_input({2, 1, 0, 0, false}));
    const float first_x = world.player(raw_welcome.player_id)->position.x;
    send_raw(pack_input({2, -1, 0, 0, false}));
    check(world.player(raw_welcome.player_id)->position.x > first_x, "duplicate input cannot replace movement");
    const float second_x = world.player(raw_welcome.player_id)->position.x;
    send_raw(pack_input({0xfffffff0u, -1, 0, 0, false}));
    check(world.player(raw_welcome.player_id)->position.x > second_x, "very old input cannot replace movement");
    const auto finite_tick = server.tick();
    server.update(std::numeric_limits<float>::infinity());
    check(server.tick() == finite_tick, "non-finite delta cannot hang server");

    const auto id = client.player_id();
    sim.pawn(id)->wood = 9;
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    guarded.poll();
    check(!guarded.connected() && guarded.player_id() == 255 && guarded.snapshot().entities.empty(),
          "silent server clears stale client state");
    server.update(0.05f);
    check(server.peers() == 0 && !world.player(id) && !sim.pawn(id), "expired peer releases world and pawn");
    client.poll();
    for (int i = 0; i < 100 && !client.connected(); ++i) {
        client.send_input(0, 0, 0, false);
        server.update(0.05f);
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(client.connected() && client.player_id() == id && sim.pawn(id)->wood == 0,
          "same endpoint reconnects with fresh inventory");
    server.close();
    check(!world.player(id) && !sim.pawn(id), "server close releases owned players");

    std::cout << "Net checks passed (port " << server.port() << ", player " << static_cast<int>(client.player_id())
              << ")\n";
}
