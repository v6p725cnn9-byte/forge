#!/usr/bin/env python3
"""Move src/engine into the reviewed folder layout and rewrite includes."""

from __future__ import annotations

import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENG = ROOT / "src" / "engine"

MOVES: dict[str, str] = {
    "core/log.hpp": "core/log/log.hpp",
    "core/jobs.hpp": "core/jobs/jobs.hpp",
    "core/jobs.cpp": "core/jobs/jobs.cpp",
    "core/profiler.hpp": "core/profiler/profiler.hpp",
    "core/time.hpp": "core/time/time.hpp",
    "core/camera.hpp": "core/camera/camera.hpp",
    "core/camera.cpp": "core/camera/camera.cpp",
    "core/paths.hpp": "core/paths/paths.hpp",
    "core/paths.cpp": "core/paths/paths.cpp",
    "rhi/command.hpp": "rhi/command/command.hpp",
    "rhi/buffer.hpp": "rhi/buffer/buffer.hpp",
    "rhi/buffer.cpp": "rhi/buffer/buffer.cpp",
    "rhi/texture.hpp": "rhi/texture/texture.hpp",
    "rhi/texture.cpp": "rhi/texture/texture.cpp",
    "rhi/shader.hpp": "rhi/shader/shader.hpp",
    "rhi/shader.cpp": "rhi/shader/shader.cpp",
    "rhi/swapchain.hpp": "rhi/swapchain/swapchain.hpp",
    "rhi/swapchain.cpp": "rhi/swapchain/swapchain.cpp",
    "rhi/handles.hpp": "rhi/device/handles.hpp",
    "rhi/resources.hpp": "rhi/device/resources.hpp",
    "rhi/resources.cpp": "rhi/device/resources.cpp",
    "rhi/upload.hpp": "rhi/device/upload.hpp",
    "rhi/upload.cpp": "rhi/device/upload.cpp",
    "rhi/host.hpp": "platform/window/host.hpp",
    "rhi/host.cpp": "platform/window/host.cpp",
    "rhi/composite.hpp": "render/passes/post/composite.hpp",
    "rhi/composite.cpp": "render/passes/post/composite.cpp",
    "assets/manager.hpp": "assets/manager/manager.hpp",
    "assets/manager.cpp": "assets/manager/manager.cpp",
    "assets/scene.hpp": "assets/gltf/scene.hpp",
    "assets/gltf.cpp": "assets/gltf/gltf.cpp",
    "assets/cube.cpp": "assets/gltf/cube.cpp",
    "assets/terrain.cpp": "assets/gltf/terrain.cpp",
    "assets/dependencies.cpp": "assets/gltf/dependencies.cpp",
    "assets/ktx2.hpp": "assets/ktx2/ktx2.hpp",
    "assets/ktx2.cpp": "assets/ktx2/ktx2.cpp",
    "render/renderer.hpp": "render/renderer/renderer.hpp",
    "render/renderer.cpp": "render/renderer/renderer.cpp",
    "render/framegraph.hpp": "render/framegraph/framegraph.hpp",
    "render/framegraph.cpp": "render/framegraph/framegraph.cpp",
    "render/pbr_pass.hpp": "render/passes/opaque/pbr_pass.hpp",
    "render/pbr_pass.cpp": "render/passes/opaque/pbr_pass.cpp",
    "render/pbr_scene.hpp": "render/passes/opaque/pbr_scene.hpp",
    "render/pbr_scene.cpp": "render/passes/opaque/pbr_scene.cpp",
    "render/ibl.hpp": "render/renderer/ibl.hpp",
    "render/ibl.cpp": "render/renderer/ibl.cpp",
    "render/survival_visuals.hpp": "render/renderer/survival_visuals.hpp",
    "render/survival_visuals.cpp": "render/renderer/survival_visuals.cpp",
    "render/settings.hpp": "render/renderer/settings.hpp",
    "render/shader_perm.hpp": "render/renderer/shader_perm.hpp",
    "anim/animator.hpp": "anim/skeleton/animator.hpp",
    "anim/animator.cpp": "anim/skeleton/animator.cpp",
    "phys/world.hpp": "physics/world/world.hpp",
    "phys/world.cpp": "physics/world/world.cpp",
    "phys/als.hpp": "physics/locomotion/als.hpp",
    "phys/als.cpp": "physics/locomotion/als.cpp",
    "world/store.hpp": "world/entities/store.hpp",
    "world/store.cpp": "world/entities/store.cpp",
    "world/city.hpp": "world/entities/city.hpp",
    "world/city.cpp": "world/entities/city.cpp",
    "world/types.hpp": "world/transform/types.hpp",
    "world/origin.hpp": "world/transform/origin.hpp",
    "world/frustum.hpp": "world/spatial/frustum.hpp",
    "world/frustum.cpp": "world/spatial/frustum.cpp",
    "world/cull.hpp": "world/spatial/cull.hpp",
    "world/cull.cpp": "world/spatial/cull.cpp",
    "world/sectors.hpp": "world/sectors/sectors.hpp",
    "world/sectors.cpp": "world/sectors/sectors.cpp",
    "world/world.hpp": "world/world.hpp",
    "game/world.hpp": "game/world/world.hpp",
    "game/actors.hpp": "game/actors/actors.hpp",
    "game/actors.cpp": "game/actors/actors.cpp",
    "game/items.hpp": "game/inventory/items.hpp",
    "game/session.hpp": "game/session/session.hpp",
    "game/session.cpp": "game/session/session.cpp",
    "game/save.hpp": "game/systems/save.hpp",
    "game/save.cpp": "game/systems/save.cpp",
    "net/socket.hpp": "net/transport/socket.hpp",
    "net/socket.cpp": "net/transport/socket.cpp",
    "net/stream.hpp": "net/transport/stream.hpp",
    "net/stream.cpp": "net/transport/stream.cpp",
    "net/channel.hpp": "net/transport/channel.hpp",
    "net/channel.cpp": "net/transport/channel.cpp",
    "net/connection.hpp": "net/connection/connection.hpp",
    "net/connection.cpp": "net/connection/connection.cpp",
    "game/net/packets.hpp": "game_net/snapshots/packets.hpp",
    "game/net/packets.cpp": "game_net/snapshots/packets.cpp",
    "game/net/interpolation.hpp": "game_net/snapshots/interpolation.hpp",
    "game/net/interpolation.cpp": "game_net/snapshots/interpolation.cpp",
    "game/net/prediction.hpp": "game_net/prediction/prediction.hpp",
    "game/net/prediction.cpp": "game_net/prediction/prediction.cpp",
    "game/net/interest.hpp": "game_net/interest/interest.hpp",
    "game/net/interest.cpp": "game_net/interest/interest.cpp",
    "game/net/game_server.hpp": "game_net/server/server.hpp",
    "game/net/game_server.cpp": "game_net/server/server.cpp",
    "game/net/game_client.hpp": "game_net/client/client.hpp",
    "game/net/game_client.cpp": "game_net/client/client.cpp",
    "script/vm.hpp": "script/vm/vm.hpp",
    "script/vm.cpp": "script/vm/vm.cpp",
    "script/api.cpp": "script/bindings/api.cpp",
    "script/registry.hpp": "script/bindings/registry.hpp",
    "audio/device.hpp": "audio/device/device.hpp",
    "audio/device.cpp": "audio/device/device.cpp",
    "ui/ui.hpp": "ui/widgets/ui.hpp",
    "ui/ui.cpp": "ui/widgets/ui.cpp",
    "ui/font.hpp": "ui/text/font.hpp",
    "ui/font.cpp": "ui/text/font.cpp",
    "ui/backdrop.hpp": "render/passes/ui/backdrop.hpp",
    "ui/backdrop.cpp": "render/passes/ui/backdrop.cpp",
    "app/menu.hpp": "frontend/menu/menu.hpp",
    "app/menu.cpp": "frontend/menu/menu.cpp",
    "app/menu_scene.hpp": "frontend/menu/menu_scene.hpp",
    "app/menu_scene.cpp": "frontend/menu/menu_scene.cpp",
    "app/background.hpp": "frontend/menu/background.hpp",
    "app/background.cpp": "frontend/menu/background.cpp",
    "app/i18n.hpp": "frontend/menu/i18n.hpp",
    "app/settings.hpp": "frontend/settings/settings.hpp",
    "app/settings.cpp": "frontend/settings/settings.cpp",
    "app/inventory_menu.hpp": "frontend/inventory/inventory_menu.hpp",
    "app/inventory_menu.cpp": "frontend/inventory/inventory_menu.cpp",
}

DIRS = [
    "core/memory", "core/jobs", "core/log", "core/profiler", "core/time", "core/camera", "core/paths",
    "platform/window", "platform/input",
    "rhi/device", "rhi/command", "rhi/buffer", "rhi/texture", "rhi/sampler", "rhi/pipeline",
    "rhi/shader", "rhi/swapchain",
    "assets/manager", "assets/gltf", "assets/ktx2", "assets/cache", "assets/streaming",
    "render/renderer", "render/framegraph",
    "render/passes/shadow", "render/passes/depth", "render/passes/opaque",
    "render/passes/transparent", "render/passes/post", "render/passes/ui",
    "anim/skeleton", "anim/clip", "anim/graph", "anim/pose", "anim/ik",
    "physics/world", "physics/character", "physics/locomotion",
    "world/entities", "world/transform", "world/spatial", "world/sectors",
    "game/world", "game/actors", "game/inventory", "game/crafting", "game/interaction",
    "game/session", "game/systems",
    "net/transport", "net/protocol", "net/connection",
    "game_net/server", "game_net/client", "game_net/snapshots", "game_net/prediction",
    "game_net/reconciliation", "game_net/interest",
    "script/vm", "script/bindings",
    "audio/device", "audio/mixer", "audio/spatial",
    "ui/widgets", "ui/layout", "ui/text",
    "frontend/menu", "frontend/settings", "frontend/inventory",
]

# old include -> new include (longest first)
INCLUDES = {
    "engine/game/net/game_client.hpp": "engine/game_net/client/client.hpp",
    "engine/game/net/game_server.hpp": "engine/game_net/server/server.hpp",
    "engine/game/net/interpolation.hpp": "engine/game_net/snapshots/interpolation.hpp",
    "engine/game/net/prediction.hpp": "engine/game_net/prediction/prediction.hpp",
    "engine/game/net/interest.hpp": "engine/game_net/interest/interest.hpp",
    "engine/game/net/packets.hpp": "engine/game_net/snapshots/packets.hpp",
    "engine/net/client.hpp": "engine/game_net/client/client.hpp",
    "engine/net/server.hpp": "engine/game_net/server/server.hpp",
    "engine/net/protocol.hpp": "engine/game_net/snapshots/packets.hpp",
    "engine/net/interest.hpp": "engine/game_net/interest/interest.hpp",
    "engine/net/channel.hpp": "engine/net/transport/channel.hpp",
    "engine/net/socket.hpp": "engine/net/transport/socket.hpp",
    "engine/net/stream.hpp": "engine/net/transport/stream.hpp",
    "engine/net/connection.hpp": "engine/net/connection/connection.hpp",
    "engine/phys/world.hpp": "engine/physics/world/world.hpp",
    "engine/phys/als.hpp": "engine/physics/locomotion/als.hpp",
    "engine/rhi/command.hpp": "engine/rhi/command/command.hpp",
    "engine/rhi/buffer.hpp": "engine/rhi/buffer/buffer.hpp",
    "engine/rhi/texture.hpp": "engine/rhi/texture/texture.hpp",
    "engine/rhi/shader.hpp": "engine/rhi/shader/shader.hpp",
    "engine/rhi/swapchain.hpp": "engine/rhi/swapchain/swapchain.hpp",
    "engine/rhi/handles.hpp": "engine/rhi/device/handles.hpp",
    "engine/rhi/resources.hpp": "engine/rhi/device/resources.hpp",
    "engine/rhi/upload.hpp": "engine/rhi/device/upload.hpp",
    "engine/rhi/host.hpp": "engine/platform/window/host.hpp",
    "engine/rhi/composite.hpp": "engine/render/passes/post/composite.hpp",
    "engine/assets/manager.hpp": "engine/assets/manager/manager.hpp",
    "engine/assets/scene.hpp": "engine/assets/gltf/scene.hpp",
    "engine/assets/ktx2.hpp": "engine/assets/ktx2/ktx2.hpp",
    "engine/render/renderer.hpp": "engine/render/renderer/renderer.hpp",
    "engine/render/framegraph.hpp": "engine/render/framegraph/framegraph.hpp",
    "engine/render/pbr_pass.hpp": "engine/render/passes/opaque/pbr_pass.hpp",
    "engine/render/pbr_scene.hpp": "engine/render/passes/opaque/pbr_scene.hpp",
    "engine/render/ibl.hpp": "engine/render/renderer/ibl.hpp",
    "engine/render/survival_visuals.hpp": "engine/render/renderer/survival_visuals.hpp",
    "engine/render/settings.hpp": "engine/render/renderer/settings.hpp",
    "engine/render/shader_perm.hpp": "engine/render/renderer/shader_perm.hpp",
    "engine/anim/animator.hpp": "engine/anim/skeleton/animator.hpp",
    "engine/world/store.hpp": "engine/world/entities/store.hpp",
    "engine/world/city.hpp": "engine/world/entities/city.hpp",
    "engine/world/types.hpp": "engine/world/transform/types.hpp",
    "engine/world/origin.hpp": "engine/world/transform/origin.hpp",
    "engine/world/frustum.hpp": "engine/world/spatial/frustum.hpp",
    "engine/world/cull.hpp": "engine/world/spatial/cull.hpp",
    "engine/world/sectors.hpp": "engine/world/sectors/sectors.hpp",
    "engine/game/world.hpp": "engine/game/world/world.hpp",
    "engine/game/actors.hpp": "engine/game/actors/actors.hpp",
    "engine/game/items.hpp": "engine/game/inventory/items.hpp",
    "engine/game/session.hpp": "engine/game/session/session.hpp",
    "engine/game/save.hpp": "engine/game/systems/save.hpp",
    "engine/script/vm.hpp": "engine/script/vm/vm.hpp",
    "engine/script/registry.hpp": "engine/script/bindings/registry.hpp",
    "engine/audio/device.hpp": "engine/audio/device/device.hpp",
    "engine/ui/ui.hpp": "engine/ui/widgets/ui.hpp",
    "engine/ui/font.hpp": "engine/ui/text/font.hpp",
    "engine/ui/backdrop.hpp": "engine/render/passes/ui/backdrop.hpp",
    "engine/app/menu.hpp": "engine/frontend/menu/menu.hpp",
    "engine/app/menu_scene.hpp": "engine/frontend/menu/menu_scene.hpp",
    "engine/app/background.hpp": "engine/frontend/menu/background.hpp",
    "engine/app/i18n.hpp": "engine/frontend/menu/i18n.hpp",
    "engine/app/settings.hpp": "engine/frontend/settings/settings.hpp",
    "engine/app/inventory_menu.hpp": "engine/frontend/inventory/inventory_menu.hpp",
    "engine/core/log.hpp": "engine/core/log/log.hpp",
    "engine/core/jobs.hpp": "engine/core/jobs/jobs.hpp",
    "engine/core/profiler.hpp": "engine/core/profiler/profiler.hpp",
    "engine/core/time.hpp": "engine/core/time/time.hpp",
    "engine/core/camera.hpp": "engine/core/camera/camera.hpp",
    "engine/core/paths.hpp": "engine/core/paths/paths.hpp",
}


def rewrite_text(text: str) -> str:
    for old, new in sorted(INCLUDES.items(), key=lambda kv: -len(kv[0])):
        text = text.replace(f'#include "{old}"', f'#include "{new}"')
    return text


def main() -> None:
    for d in DIRS:
        (ENG / d).mkdir(parents=True, exist_ok=True)

    for src, dst in MOVES.items():
        a, b = ENG / src, ENG / dst
        if not a.exists():
            print("missing", src)
            continue
        b.parent.mkdir(parents=True, exist_ok=True)
        if a.resolve() == b.resolve():
            continue
        if b.exists():
            print("exists", dst)
            continue
        shutil.move(str(a), str(b))
        print("moved", src, "->", dst)

    # drop leftover shims under net/ that pointed at game/net
    for leftover in ("net/client.hpp", "net/server.hpp", "net/protocol.hpp", "net/interest.hpp", "frontend/menu.hpp"):
        p = ENG / leftover
        if p.exists():
            p.unlink()
            print("removed", leftover)

    for path in list(ROOT.joinpath("src").rglob("*")) + list(ROOT.joinpath("tests").rglob("*")):
        if path.suffix not in {".hpp", ".cpp", ".h", ".c"}:
            continue
        text = path.read_text(encoding="utf-8")
        new = rewrite_text(text)
        if new != text:
            path.write_text(new, encoding="utf-8")
            print("includes", path.relative_to(ROOT))


if __name__ == "__main__":
    main()
