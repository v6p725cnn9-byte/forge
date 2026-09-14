#include "engine/anim/graph/graph.hpp"
#include "engine/audio/mixer/mixer.hpp"
#include "engine/audio/spatial/spatial.hpp"
#include "engine/core/jobs/jobs.hpp"
#include "engine/core/memory/arena.hpp"
#include "engine/core/time/time.hpp"
#include "engine/game/crafting/crafting.hpp"
#include "engine/game_net/prediction/prediction.hpp"
#include "engine/game_net/reconciliation/reconciliation.hpp"
#include "engine/game/systems/save.hpp"
#include "engine/game/world/world.hpp"
#include "engine/net/protocol/envelope.hpp"
#include "engine/rhi/device/handles.hpp"
#include "engine/physics/character/character.hpp"
#include "engine/platform/input/input.hpp"
#include "engine/render/framegraph/framegraph.hpp"
#include "engine/render/renderer/shader_perm.hpp"
#include "engine/ui/layout/layout.hpp"
#include "engine/world/transform/origin.hpp"
#include "engine/world/sectors/sectors.hpp"
#include "engine/world/scenery/store.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <glm/glm.hpp>
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
} // namespace

int main()
{
    using namespace forge;

    check(core::kSimHz == 20 && core::kNetHz == 20, "fixed sim/net tick");
    core::Arena arena(1024);
    check(arena.allocate(16) != nullptr && arena.used() >= 16, "arena bump");
    check(anim::select_locomotion(0) == anim::Locomotion::Idle, "anim graph idle");
    check(net::envelope_bytes() == 17, "transport envelope");
    check(game::recipe(0) != nullptr, "crafting table");
    check(audio::attenuation({0, 0, 0}, {100, 0, 0}) == 0.0f, "audio distance");
    audio::mixer().set_master(0.5f);
    check(audio::mixer().master() == 0.5f, "mixer master");
    const auto padded = ui::pad({0, 0, 10, 10}, 2);
    check(padded.w == 6 && padded.h == 6, "ui layout pad");
    platform::Input local{};
    check(!local.jump, "platform input");
    check(render::pbr_vertex_shader(render::ShaderFeature::Skinned) == std::string("pbr_skinned.vert"),
          "skinned permutation");
    check(render::pbr_vertex_shader(render::ShaderFeature::Instanced) == std::string("pbr_instanced.vert"),
          "instanced permutation");

    render::FrameGraph bad;
    bad.add_pass("opaque", {render::FrameGraph::kInvalid}, {render::FrameGraph::kInvalid},
                 [](rhi::Command&) { return true; });
    check(!bad.compile(), "pass with missing resource fails compile");
    render::FrameGraph graph;
    auto* fake = reinterpret_cast<SDL_GPUTexture*>(static_cast<std::uintptr_t>(1));
    auto hdr = graph.import("hdr", fake);
    auto swap = graph.import("swapchain", fake);
    graph.add_pass("opaque", {hdr}, {hdr}, [](rhi::Command&) { return true; });
    graph.add_pass("tonemap", {hdr}, {swap}, [](rhi::Command&) { return true; });
    check(graph.compile(), "imported hdr can feed tonemap");
    check(graph.first_use(hdr) == 0 && graph.last_use(hdr) == 1, "hdr lifetime spans both passes");
    check(graph.dead_after(hdr, 2), "hdr is dead after last use");
    const auto bloom = graph.create_transient({"bloom", 64, 64});
    check(graph.transient(bloom), "bloom is a transient");
    check(net::kDefaultSnapshotEntities == 32 && net::kMaxSnapshotEntities == 128, "snapshot caps split");
    check(net::lane_of(net::Reliability::Reliable) == net::Lane::ReliableUnordered, "reliable lane");
    rhi::TextureHandle dead{};
    check(!dead, "zero generation handle is empty");

    core::JobPool pool(2);
    std::atomic<int> hits{0};
    pool.enqueue([&] { hits.fetch_add(1); });
    pool.enqueue([&] { hits.fetch_add(1); });
    pool.wait();
    check(hits.load() == 2, "job pool ran both tasks");

    net::Prediction prediction;
    net::Input input;
    input.seq = 1;
    input.move_z = 1;
    prediction.reset({0, 1, 0}, 0);
    prediction.record(input, 0.05f);
    check(prediction.position().z > 0.1f, "prediction integrates ALS run speed");
    prediction.reconcile({0, 1, 0}, 0, 1);
    check(std::abs(prediction.position().z) < 0.01f, "acked input is not replayed");

    game::World world;
    world.sim.reset();
    world.sim.ensure_pawn(0);
    auto blob = game::save::capture(world.sim);
    auto bytes = game::save::encode(blob);
    game::save::Blob decoded;
    check(game::save::decode(bytes.data(), bytes.size(), decoded), "save round-trip");
    check(decoded.pawns[0].used, "saved pawn survives encode");

    world::Store store;
    store.spawn({0, 0, 0}, 0, {1, 1, 1}, 0, {1, 1, 1, 1});
    world::SectorIndex index;
    index.build(store);
    world::StreamStats stats;
    index.stream(store, {0, 0, 0}, 64, stats);
    check(index.residency(store.sectors[0]) == world::Residency::Resident, "streamed sector is resident");

    world::Origin origin{{10, 0, 0}};
    check(origin.to_local({12, 0, 0}).x == 2.0f, "sector-relative origin");

    check(net::kMaxPacket == net::kMaxGamePayload && net::kMaxDatagram == 1400, "packet budget aliases");
    check(net::kTransportHeader == 17 && net::kGameHeader == 8, "header sizes");

    std::cout << "Architecture checks passed\n";
    return 0;
}
