# Forge — agent rules

C++20 game engine. Develop on macOS (Metal via SDL3 GPU), ship Windows later (DX12/Vulkan via the same API).

## Non-negotiables

- This is **our** engine. Do not pull in Unity, Unreal, Godot, or Bevy.
- Do **not** write physics, glTF parsing, texture compression, or audio mixing from scratch. Use Jolt, cgltf, KTX2/Basis, miniaudio/SDL audio when those milestones open.
- PBR is clustered later; M2 is forward PBR + IBL + ACES. Do not skip the HDR offscreen target.
- Do **not** add a second GPU backend by hand. SDL3 GPU is the RHI. M0 is MSL; later shadercross compiles HLSL → MSL/SPIR-V/DXIL.
- Visual floor: above SA-MP. Target: GTA IV / Sleeping Dogs class (PBR, CSM, HDR), not Unreal 5 Nanite/Lumen.
- Data-oriented: SoA, arenas, no `GameObject` trees as the world model.

## Roadmap is the contract

The source of truth for what is done lives in `README.md` as **TOMLMD**:

1. TOML block between `+++` lines at the top of the README.
2. Matching GitHub checkboxes in the body (`- [ ] \`task-id\``).

When you finish a task:

```bash
python3 scripts/progress.py close <task-id>
```

That flips `status` in the TOML **and** the markdown checkbox. Do not leave them out of sync. If you complete work without an id, add a `[[tasks]]` row first.

Check remaining work:

```bash
python3 scripts/progress.py
```

## Code

- C++20, `-Wall -Wextra -Wpedantic`.
- Engine code lives under `src/engine/`. Keep `main.cpp` thin.
- SDL resources are owned by `forge::rhi::Device` (or the future module that replaces it). No leaked command buffers: every `SDL_AcquireGPUCommandBuffer` is submitted or cancelled.
- Comments only for non-obvious constraints, in English.
- Prefer small, compiling steps over empty stubs that pretend a system exists.

## Build

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
./build/forge       # latest lab
./build/forge_m1    # keep forever
./build/forge_m2
./build/forge_m3
./build/forge_m4
./build/forge_m5
./build/forge_m6
./build/forge_m7
```

Milestone labs live in `tests/mN/`. Do not delete an old lab when adding a new one. `forge` always aliases the latest lab.
