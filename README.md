+++
schema = "tomlmd/v1"
project = "forge"
version = "0.9.0"
active_milestone = "G1"
bar = "above-samp"
visual_target = "gta-iv / sleeping-dogs class"

# status: todo | doing | done | blocked
# close a task: python3 scripts/progress.py close <id>
# that updates this TOML and the checkboxes below. keep them in sync.

[[milestones]]
id = "M0"
title = "Bootstrap"
status = "doing"

[[milestones]]
id = "M1"
title = "RHI + camera"
status = "done"

[[milestones]]
id = "M2"
title = "PBR glTF"
status = "done"

[[milestones]]
id = "M3"
title = "Shadows + Jolt character"
status = "done"

[[milestones]]
id = "M4"
title = "World streaming"
status = "done"

[[milestones]]
id = "M5"
title = "Animation + vehicle"
status = "done"

[[milestones]]
id = "M6"
title = "Lua gamemode API"
status = "done"

[[milestones]]
id = "M7"
title = "Client-server"
status = "done"

[[milestones]]
id = "G1"
title = "Survival session and menu"
status = "done"

[[tasks]]
id = "m0-repo"
milestone = "M0"
status = "done"
title = "Repo, gitignore, layout"

[[tasks]]
id = "m0-cmake"
milestone = "M0"
status = "done"
title = "CMake + Ninja presets (macOS)"

[[tasks]]
id = "m0-agents"
milestone = "M0"
status = "done"
title = "AGENTS.md conventions"

[[tasks]]
id = "m0-readme"
milestone = "M0"
status = "done"
title = "README TOMLMD roadmap"

[[tasks]]
id = "m0-progress"
milestone = "M0"
status = "done"
title = "scripts/progress.py close/list"

[[tasks]]
id = "m0-window"
milestone = "M0"
status = "done"
title = "SDL3 window, resize, HiDPI, Esc/Quit"

[[tasks]]
id = "m0-device"
milestone = "M0"
status = "done"
title = "SDL3 GPU device, log backend name"

[[tasks]]
id = "m0-triangle"
milestone = "M0"
status = "done"
title = "MSL triangle, swapchain present, FPS in title"

[[tasks]]
id = "m0-verify"
milestone = "M0"
status = "done"
title = "Debug build runs on this Mac (Metal)"

[[tasks]]
id = "m0-windows"
milestone = "M0"
status = "todo"
title = "Windows preset (DX12) — later, needs shadercross"

[[tasks]]
id = "g1-astronaut-walk-alignment"
milestone = "G1"
status = "done"
title = "Align imported astronaut walk, remove hip root motion and attach player label"

[[tasks]]
id = "g1-camera-person"
milestone = "G1"
status = "done"
title = "First-person view alongside third-person, Lua camera.setFirstPerson/setThirdPerson/getCurrentPerson"

[[tasks]]
id = "g1-jump-headless"
milestone = "G1"
status = "done"
title = "Server-authoritative jump over UDP and CS2-style headless first-person body"

[[tasks]]
id = "g1-strafe-facing"
milestone = "G1"
status = "done"
title = "Body faces the camera while strafing and backpedaling, no 180-degree flip on S"

[[tasks]]
id = "g1-diagonal-walk"
milestone = "G1"
status = "done"
title = "Normalize diagonal WASD so packets pass input validation and diagonals move"

[[tasks]]
id = "m0-windows-local-launch"
milestone = "M0"
status = "done"
title = "Local Windows MSVC/SDL3 build, portable shader paths, Winsock length type and DX12 smoke"

[[tasks]]
id = "m1-shader-load"
milestone = "M1"
status = "done"
title = "Shader loader by format (MSL/SPIR-V/DXIL)"

[[tasks]]
id = "m1-hlsl"
milestone = "M1"
status = "done"
title = "HLSL source + SDL_shadercross offline cook"

[[tasks]]
id = "m1-vbo"
milestone = "M1"
status = "done"
title = "Vertex + index buffers, not vertex_id"

[[tasks]]
id = "m1-depth"
milestone = "M1"
status = "done"
title = "Depth texture, depth test/write"

[[tasks]]
id = "m1-ubo"
milestone = "M1"
status = "done"
title = "Uniform MVP, perspective camera"

[[tasks]]
id = "m1-flycam"
milestone = "M1"
status = "done"
title = "Fly camera WASD + mouse"

[[tasks]]
id = "m1-imgui"
milestone = "M1"
status = "done"
title = "Dear ImGui overlay (dev only)"

[[tasks]]
id = "m1-verify"
milestone = "M1"
status = "done"
title = "Camera/asset tests, Debug + Release GPU smoke with resize and failure checks"

[[tasks]]
id = "m2-gltf"
milestone = "M2"
status = "done"
title = "glTF 2.0 mesh load (cgltf)"

[[tasks]]
id = "m2-pbr"
milestone = "M2"
status = "done"
title = "Metallic-roughness + normals + albedo"

[[tasks]]
id = "m2-dirlight"
milestone = "M2"
status = "done"
title = "Directional light, basic BRDF"

[[tasks]]
id = "m2-ibl"
milestone = "M2"
status = "done"
title = "IBL cubemap"

[[tasks]]
id = "m2-tonemap"
milestone = "M2"
status = "done"
title = "HDR + ACES tonemap"

[[tasks]]
id = "m2-ktx2"
milestone = "M2"
status = "done"
title = "KTX2/Basis texture cook"

[[tasks]]
id = "m3-labs"
milestone = "M3"
status = "done"
title = "Keep milestone binaries tests/m1 m2 m3"

[[tasks]]
id = "m3-csm"
milestone = "M3"
status = "done"
title = "Cascaded shadow maps"

[[tasks]]
id = "m3-jolt"
milestone = "M3"
status = "done"
title = "Jolt world, static collider from mesh"

[[tasks]]
id = "m3-controller"
milestone = "M3"
status = "done"
title = "Capsule character controller"

[[tasks]]
id = "m3-bloom"
milestone = "M3"
status = "done"
title = "Bloom (after HDR works)"

[[tasks]]
id = "m4-sectors"
milestone = "M4"
status = "done"
title = "World sectors / IPL-like streaming"

[[tasks]]
id = "m4-instance"
milestone = "M4"
status = "done"
title = "GPU instancing for props"

[[tasks]]
id = "m4-frustum"
milestone = "M4"
status = "done"
title = "Frustum cull"

[[tasks]]
id = "m4-soa"
milestone = "M4"
status = "done"
title = "SoA entity storage, not a scene graph"

[[tasks]]
id = "m5-skin"
milestone = "M5"
status = "done"
title = "glTF skins, one walking character"

[[tasks]]
id = "m5-vehicle"
milestone = "M5"
status = "done"
title = "One driveable vehicle (Jolt)"

[[tasks]]
id = "m6-lua"
milestone = "M6"
status = "done"
title = "Embed Lua 5.4 / Luau"

[[tasks]]
id = "m6-api"
milestone = "M6"
status = "done"
title = "spawn player/vehicle, marker, 3D label"

[[tasks]]
id = "m7-udp"
milestone = "M7"
status = "done"
title = "UDP client-server tick"

[[tasks]]
id = "m7-stream"
milestone = "M7"
status = "done"
title = "Interest management / stream distance"

[[tasks]]
id = "g1-session"
milestone = "G1"
status = "done"
title = "Survival session, solo/host/join, settings and localized UI"

[[tasks]]
id = "g1-menu-vista"
milestone = "G1"
status = "done"
title = "Procedural HDR menu vista, fog and image fallback"

[[tasks]]
id = "g1-stability"
milestone = "G1"
status = "done"
title = "Network validation/reconnect, GPU cleanup and regression coverage"
+++

# Forge

Свой 3D-движок. Не Unity, не Godot, не форк GTA.

**Планка:** выше SA-MP. **Цель по картинке:** GTA IV / Sleeping Dogs — PBR, каскадные тени, HDR, скелеты. Не Doom и не Unreal 5.

**Стек:** C++20 · SDL3 GPU (Metal / DX12 / Vulkan) · GLM · cgltf · libktx · Jolt · Lua 5.4.

Сейчас: `forge` открывает главное меню (соло / мультиплеер / настройки / выход). Соло и хост — одна симуляция; друг заходит из меню или `FORGE_CONNECT`. Настройки (графика, управление, звук, язык) пишутся в `settings.cfg`. Лабы `forge_m1`…`forge_m7` остаются.

Открытый хвост движка — Windows/DX12 (`m0-windows`).

## TOMLMD

Дорожная карта — TOML в шапке этого файла. Чекбоксы ниже зеркалят `[[tasks]]`.

```bash
python3 scripts/progress.py              # список
python3 scripts/progress.py doing m1-vbo # в работе
python3 scripts/progress.py close m1-vbo # закрыть
python3 scripts/progress.py reopen m1-vbo
```

`close` ставит `status = "done"` и `- [x]`. Не редактируй одно без другого.

## Сборка

Нужны CMake ≥ 3.24, Ninja, Python ≥ 3.9, Git, C++20 и SDL3 ≥ 3.2 (на этом Маке: Homebrew `sdl3`). Первая конфигурация тянет GLM, ImGui, libktx и Jolt Physics v5.6.0 через FetchContent.

GLM — математика. SDL3 GPU — единственный RHI. ImGui — официальный SDL GPU backend для диагностики и подписей старых лаб. libktx нужен для Basis/UASTC → ASTC/BC7/RGBA. Компилятор шейдеров в runtime не входит.

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
./build/forge            # главное меню → сессия
./build/forge_survival   # same
FORGE_CONNECT=192.168.0.10:27015 ./build/forge   # сразу join, без меню
./build/forge_m7         # UDP demo
./build/forge_m1    # unlit cubes
./build/forge_m2    # PBR helmet
./build/forge_m3    # shadows + Jolt walk
./build/forge_m4    # streaming city
./build/forge_m5    # skinned fox + car
./build/forge_m6    # Lua gamemode
./build/forge_m7    # UDP listen-server
```

RMB — взгляд. M1/M2/M4: WASD полёт. M3: **F** ходьба/полёт, Space прыжок. M5/M6: WASD ходьба, **F** сесть/высадиться. M7: WASD по сети, боты стримятся по радиусу. F1 — панель. `FORGE_MODEL` подменяет glTF. `FORGE_GAMEMODE` подменяет скрипт. `FORGE_PORT` — UDP-порт (по умолчанию 27015).

Release без панели диагностики и GPU debug validation:

```bash
cmake --preset macos-release
cmake --build --preset macos-release
./build-release/forge
```

`FORGE_DEV_UI=OFF` отключает панель диагностики. ImGui остаётся в зависимостях для HUD и подписей старых лаб; меню и HUD survival используют собственный UI. Shader assets ищутся рядом с исполняемым файлом, поэтому запуск не зависит от рабочего каталога.

### Проверки

```bash
ctest --preset macos-debug
python3 scripts/smoke.py ./build/forge
python3 scripts/smoke.py ./build-release/forge
```

CTest: камера, glTF, SHA шейдеров и KTX2, SoA/сектора/frustum, скины, Lua, UDP loopback + interest. Smoke требует графической сессии: 90 кадров, resize HDR-цели, невалидный `FORGE_SMOKE` и запуск без ресурсов. Включить в CTest: `cmake --preset macos-debug -DFORGE_GPU_TESTS=ON`.

Короткий прогон: `FORGE_SMOKE=60 ./build/forge`. Успех означает N действительно отправленных на показ кадров; ошибки рендера, досрочное закрытие или 10 секунд без кадров дают ненулевой код выхода. `FORGE_SMOKE_RESIZE=1` включает изменение размера на 10-м и 30-м кадрах.

### Шейдеры

Исходники: `shaders/pbr.vert.hlsl`, `shaders/pbr_instanced.vert.hlsl`, `shaders/pbr_skinned.vert.hlsl`, `shaders/pbr.frag.hlsl`, `shaders/tonemap.vert.hlsl`, `shaders/tonemap.frag.hlsl`. Готовые MSL/SPIR-V/DXIL лежат в `shaders/cooked/`. Сборка сверяет SHA-256; устаревший cook останавливается с инструкцией.

Текстуры шлема: PNG в `assets/models/FlightHelmet/`, KTX2 рядом. Проверка: `python3 scripts/cook_textures.py --check`. Пересборка: нужен `toktx` (локально `.cache/ktx-build/Release/toktx`).

Для изменения шейдеров один раз собери локальный инструмент (требуются сеть, C/C++ toolchain и несколько минут):

```bash
python3 scripts/build_shadercross.py
python3 scripts/cook_shaders.py
cmake --build --preset macos-debug
```

Или передай существующий CLI: `python3 scripts/cook_shaders.py --shadercross /path/to/shadercross`. Скрипт выпускает все три формата и JSON reflection, проверяет число GPU-ресурсов и сохраняет предыдущий комплект при ошибке компиляции. При изменении layout ресурсов обнови `ShaderSpec`, описание вершин и проверку reflection вместе.

SDL_shadercross закреплён на `1ff05bec573988a98ef9e0260b4da44f512b8367`; DXC и SPIRV-Cross собираются в `.cache/`. [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) · [cgltf](https://github.com/jkuhlmann/cgltf) · [KTX-Software](https://github.com/KhronosGroup/KTX-Software) · [Dear ImGui SDL3 GPU](https://github.com/ocornut/imgui/tree/v1.92.9b/examples/example_sdl3_sdlgpu3) · [GLM](https://github.com/g-truc/glm/tree/1.0.3).

Windows (DX12) — задача `m0-windows`, остаётся открытой до настройки пресета и проверки на Windows. DXIL уже генерируется, но наличие байткода не заменяет проверку runtime.

## Дерево

```
src/engine/core/     камера, пути
src/engine/assets/   cgltf, MikkTSpace, skins, unit cube
src/engine/anim/     сэмплинг клипов, joint palette
src/engine/script/   Lua 5.4 VM, spawn API
src/engine/net/      UDP сокет, протокол, interest
src/engine/rhi/      host, SDL3 GPU, KTX2
src/engine/render/   PBR, IBL, CSM, bloom, instancing, GPU skinning
src/engine/world/    SoA store, сектора, frustum
src/engine/phys/     Jolt world, CharacterVirtual, wheeled vehicle
src/engine/app/      общий цикл лаб
tests/m1             unlit cubes (остаётся)
tests/m2             PBR helmet (остаётся)
tests/m3             shadows + walk (остаётся)
tests/m4             streaming city (остаётся)
tests/m5             fox + car (остаётся)
tests/m6             Lua gamemode (остаётся)
tests/m7             UDP listen-server
tests/survival       главное меню и survival (latest)
assets/scripts/      main.lua, net.lua
shaders/             HLSL + cooked MSL/SPIR-V/DXIL
```

## Не пишем сами

Физика (Jolt), парсер glTF (cgltf), сжатие текстур (KTX2), оконный слой (SDL3), три GPU API (SDL3 GPU). Своё — мир, стриминг, пайплайн кадра, репликация, Lua API.

---

### M0 Bootstrap

- [x] `m0-repo` — репозиторий, gitignore, раскладка
- [x] `m0-cmake` — CMake + Ninja пресеты (macOS)
- [x] `m0-agents` — AGENTS.md
- [x] `m0-readme` — README TOMLMD
- [x] `m0-progress` — `scripts/progress.py`
- [x] `m0-window` — SDL3 окно, resize, HiDPI, Esc
- [x] `m0-device` — GPU device, лог бэкенда
- [x] `m0-triangle` — MSL-треугольник, present, fps в тайтле
- [x] `m0-verify` — Debug-сборка запускается на этом Маке
- [ ] `m0-windows` — Windows-пресет (DX12), позже
- [x] `m0-windows-local-launch` — локальная сборка MSVC/SDL3 и запуск DX12 (60 кадров с resize)

### M1 RHI + camera

- [x] `m1-shader-load` — загрузка шейдера по формату бэкенда
- [x] `m1-hlsl` — HLSL + SDL_shadercross
- [x] `m1-vbo` — vertex/index buffer
- [x] `m1-depth` — depth buffer
- [x] `m1-ubo` — MVP uniform, перспектива
- [x] `m1-flycam` — WASD + мышь
- [x] `m1-imgui` — ImGui (только dev)
- [x] `m1-verify` — тесты камеры и ресурсов; Debug/Release smoke с resize и проверкой ошибок

### M2 PBR glTF

- [x] `m2-gltf` — cgltf, меш
- [x] `m2-pbr` — metallic-roughness + normals
- [x] `m2-dirlight` — направленный свет, BRDF
- [x] `m2-ibl` — IBL cubemap
- [x] `m2-tonemap` — HDR + ACES
- [x] `m2-ktx2` — KTX2/Basis

### M3 Shadows + character

- [x] `m3-labs` — бинарники `forge_m1` / `forge_m2` / `forge_m3` остаются
- [x] `m3-csm` — cascaded shadow maps
- [x] `m3-jolt` — Jolt, статика
- [x] `m3-controller` — капсула
- [x] `m3-bloom` — bloom

### M4 World streaming

- [x] `m4-sectors` — сектора мира
- [x] `m4-instance` — инстансинг пропа
- [x] `m4-frustum` — frustum cull
- [x] `m4-soa` — SoA сущности

### M5 Animation + vehicle

- [x] `m5-skin` — glTF skins
- [x] `m5-vehicle` — одна машина на Jolt

### M6 Lua

- [x] `m6-lua` — Lua 5.4 / Luau
- [x] `m6-api` — spawn / marker / 3D text (уровень SA-MP)

### M7 Client-server

- [x] `m7-udp` — UDP тик
- [x] `m7-stream` — stream distance


### G1 Survival session and menu

- [x] `g1-session` — сессия, ресурсы/костёр/эвакуация, solo/host/join, настройки и RU/EN UI
- [x] `g1-menu-vista` — процедурный пейзаж, HDR, туман и резервный фон меню
- [x] `g1-stability` — проверка UDP-пакетов, переподключение, освобождение GPU-ресурсов и регрессионные тесты

Проверки стабильности: `net` проверяет чужие/устаревшие пакеты, NaN, лимит MTU,
таймаут и переподключение без наследования инвентаря; `settings` — повреждённые значения.
UI работает в координатах окна (включая Retina) и не теряет короткие клики между кадрами.
Подключение ожидает ответ до трёх секунд по реальному времени, включая Release.
GPU smoke запускает каждую лабу, меняет размер окна и проверяет ошибки старта.
Для survival дополнительно проверяются 3D-меню и отказ при некорректном адресе подключения.
Smoke использует временный файл настроек и не меняет пользовательский `settings.cfg`.

Сетевой транспорт пока экспериментальный: без аутентификации, шифрования и гарантированной
доставки одноразовых действий. Проверки на одном Mac не заменяют испытание между двумя
машинами в LAN. Аудиоползунки сохраняются, подключение аудиодвижка ещё впереди.

- [x] `g1-astronaut-walk-alignment` — направление космонавта, ходьба на месте и привязка ника
- [x] `g1-camera-person` — first-person рядом с third-person, Lua camera.setFirstPerson/setThirdPerson/getCurrentPerson
- [x] `g1-jump-headless` — прыжок через UDP с гравитацией на сервере и тело от 1-го лица без головы как в CS2
- [x] `g1-strafe-facing` — тело смотрит за камерой, S/A/D идут стрейфом без разворота на 180°
- [x] `g1-diagonal-walk` — нормализация диагоналей WASD, пакеты проходят валидацию, диагонали двигают
