# Forge — архитектура

C++20 игровой движок. Не Unity / Unreal / Godot / Bevy.

| Слой | Технология |
|---|---|
| GPU / окно | SDL3 GPU. На macOS Metal (MSL). Позже DX12/Vulkan тем же API через shadercross |
| Физика коллизий | Jolt `CharacterVirtual` + боксы. Не писать физику с нуля |
| Локомоция | standalone C++20 по концепциям ALS-Refactored (MIT), без Unreal. Коллизии — Jolt |
| Ассеты | glTF 2.0 (`cgltf`), KTX2, Mixamo X Bot |
| Скрипты | Lua 5.4 |
| Мир | SoA, не дерево `GameObject` |
| Сеть | свой UDP: транспорт в `engine/net/`, репликация в `engine/game/net/` |

Технический baseline (не равенство картинки): GTA IV / Sleeping Dogs class — forward PBR, CSM, HDR, IBL, ACES. Не Nanite/Lumen. SSAO, TAA, volumetric fog и material authoring — впереди.

Контракт «что сделано» живёт в `README.md` (TOML + чекбоксы). Закрытие задачи:

```bash
python3 scripts/progress.py close <task-id>
```

---

## Дерево репозитория

```
forge/
├── AGENTS.md                 правила для агентов
├── ARCHITECTURE.md           этот файл
├── README.md                 роадмап TOMLMD (единственный статус «готово»)
├── CMakeLists.txt            цели, FetchContent, stage ассетов
├── CMakePresets.json         macos-debug → build/, macos-release → build-release/
├── src/engine/               весь движок (include path = src/)
├── tests/                    лабы M1–M7 + survival + юнит-тесты
├── shaders/                  HLSL-исходники + cooked/ (MSL, SPIR-V, DXIL)
├── assets/                   модели, шрифты, Lua, эффекты
├── scripts/                  cook, progress, smoke
├── third_party/              cgltf, mikktspace, stb (без движка)
└── build/                    артефакты debug (не в git)
```

Заголовки живут рядом с `.cpp` под `src/engine/...`. Инклуды вида `#include "engine/rhi/host.hpp"`.

---

## Как крутится кадр

```
бинарник (forge / forge_mN)
  tests/*/main.cpp              тонкий main
  tests/*/lab.cpp               конкретная сцена : app::Lab
       │
       ▼
  app::run_lab()                окно, ввод, цикл          [app/run.cpp]
  app::run_game()               меню → сессия             [tests/survival/shell.cpp]
       │
       ▼
  rhi::Host                     окно + GPU device + swapchain
  render::Renderer              HDR / depth / bloom, FrameGraph post
       │
       ├─ Lab::update(dt)       симуляция 20 Hz, сеть 20 Hz, камера
       └─ Lab::draw()           command buffer
                                  opaque → HDR
                                  FrameGraph: bloom → ACES tonemap
                                  shipping UI / debug ImGui overlay
```

Актуальный «игра» бинарь — `forge` (= `forge_survival`): меню → сессия → `game::Sim` на хосте, `net::Client` у всех, картинка в `render::SurvivalVisuals`.

`forge` всегда алиас последней лабы. Старые `forge_mN` не удаляются.

---

## Слои `src/engine/`

Зависимости сверху вниз (стрелка = «линкует / включает»):

```
Application (app::run_lab / run_game)
        │
   ┌────┴────┐
   │         │
frontend   Game (actors + Sim)
   │         │
   │    ┌────┴────┐
   │    │         │
Renderer  Simulation  Network
   │    │         │
  RHI  Physics   Transport
   │    │         │
   └────┴─────────┘
         Core (log, jobs, profiler, time)
```

Один корень: `game::World` (`world.actors`, `world.sim`). Lua только вызывает spawn/set. Сеть читает тот же World. `world/scenery` — SoA декора, не gameplay entities.

CMake это отражает отдельными статическими либами: `forge_net` не знает про игру; `forge_game_net` знает.

---

## Дерево `src/engine/`

```
src/engine/
  core/
    memory/          bump Arena
    jobs/            thread pool
    log/             каналы CORE…AUDIO, TRACE…ERROR
    profiler/        CPU scopes
    time/            kSimHz / kNetHz = 20
    camera/          fly / 1st / 3rd
    paths/           shaders/assets рядом с бинарём
    scope_exit.hpp
  platform/
    window/          Window + rhi::Host (окно + claim GPU)
    input/           WASD / jump / interact
  rhi/
    device/          Device, handles, resources, upload
    command/         RAII command buffer
    buffer/
    texture/         GPU upload (не KTX decode)
    sampler/
    pipeline/
    shader/          cooked MSL/SPIR-V/DXIL
    swapchain/
  assets/
    manager/         catalog() кэш glTF
    gltf/            scene, cgltf, cube, terrain
    ktx2/            decode/transcode
    cache/           alias catalog
    streaming/       residency → CPU/GPU
  render/
    renderer/        HDR/depth/bloom, IBL, survival visuals
    framegraph/
    passes/
      shadow/
      depth/
      opaque/        PBR / skinned / instanced
      transparent/
      post/          bloom, ACES, composite
      ui/            backdrop blur
  anim/
    skeleton/        globals, palette
    clip/            find/sample
    graph/           idle/walk/run
    pose/            Palette
    ik/              collapse_joint
  physics/
    world/           Jolt
    character/       CharacterVirtual API
    locomotion/      ALS standalone
  world/
    scenery/         SoA props, city kit
    transform/       types, origin
    spatial/         frustum, cull
    sectors/         residency
  game/
    world/           aggregate root: actors + sim
    actors/
    inventory/       items
    crafting/        recipes
    interaction/     harvest
    session/         Sim
    systems/         save
  net/
    transport/       socket, stream, channel
    protocol/        FNT1 envelope
    connection/      seq/ACK/RTT
  game_net/
    server/
    client/
    snapshots/       packets, interpolation
    prediction/
    reconciliation/
    interest/
  script/
    vm/
    bindings/        Lua API, Registry alias
  audio/
    device/
    mixer/
    spatial/
  ui/
    widgets/
    layout/
    text/            MSDF
  frontend/
    menu/
    settings/
    inventory/
  app/               lifecycle: lab.hpp, run.cpp
  dev/               ImGui overlay only
```

---

## Сеть: два слоя

```
игра                              транспорт
engine/game_net/                  engine/net/
  snapshots   Snapshot              transport/socket   UDP
  server      Sim + Jolt            transport/stream   байты
  client      interpol              connection         seq/ACK/RTT
  prediction  replay                protocol           FNT1
  reconciliation                    transport/channel  reliable unordered
  interest    radius 55 м
```

Клиент шлёт Hello/Welcome **reliable unordered**, Input/Snapshot **unreliable**. Reliable ≠ ordered: 102 может завершиться раньше 101.

Packet enum зарезервирован: Hello, Welcome, Input, Snapshot, Ping, Pong, Disconnect, Event, ReliableEvent, Ack, Resync, Auth.

Budget:

| Константа | Значение |
|---|---|
| Ethernet MTU | 1500 |
| IPv4+UDP overhead | 28 |
| `kMaxDatagram` / `kMaxGamePayload` / `kMaxSnapshotSize` | 1400 |
| FNT1 transport header | 17 |
| game header | 8 |

Тики: Sim 20 Hz, Network 20 Hz, Render/Animation — переменные (дисплей). Клиент: snapshot buffer (100 ms delay) + kinematic prediction + reconcile по ack.

Авторитет — сервер. `Action` — request; сервер проверяет рецепт/инвентарь/станцию/тайминг и шлёт `Result`.

Голый UDP без транспортного конверта сервер всё ещё принимает (legacy) — так живут `net_tests`.

Порт по умолчанию: **27015**.

---

## Игровой цикл survival

Хост считает `game::Sim`:

1. День/ночь, O2, холод, радиация, HP, stamina.
2. Добыча E: дерево (топор), камень/руда (кирка), палки руками; E по игроку — удар.
3. Инвентарь / крафт / печь / верстак. Вес ≤ 45 кг.
4. Костёр горит 80 с.
5. Смерть → лут на земле, респаун 8 с (сессия не валится).
6. Эвакуация у маяка только с **4 железными слитками**.
7. Таймер 360 с: кто-то ушёл со слитками → win, никто → fail.

Клиент рисует `render::SurvivalVisuals` по snapshot: Mixamo-манекен, деревья Kenney, костёр, станции.

---

## CMake-библиотеки

Определены в корневом `CMakeLists.txt`. FetchContent: glm, Jolt v5.6, KTX-Software, Lua 5.4.7, Dear ImGui. SDL3 ищется системой (`find_package(SDL3)`), на macOS через Homebrew `/opt/homebrew`.

| Таргет | Состав | Линкует |
|---|---|---|
| `lua_static` | Lua 5.4 core | — |
| `forge_asset_deps` | mikktspace + stb/cgltf include | — |
| `forge_assets` | `assets/gltf` + `ktx2` + `manager` | glm, ktx, asset_deps |
| `forge_anim` | `anim/skeleton` | assets |
| `forge_camera` | камера | glm |
| `forge_world` | SoA мир | glm |
| `forge_imgui` | Dear ImGui + SDL3/GPU backends | SDL3 |
| `forge_physics` | Jolt world + ALS | Jolt, glm |
| `forge_script` | `script/vm` + `bindings` | lua, glm, camera, game |
| `forge_game` | actors, session, systems/save | glm, net |
| `forge_net` | `net/transport` + `connection` | ws2_32 на Windows |
| `forge_game_net` | `game_net/{server,client,snapshots,prediction,interest}` | net, script, game, physics |
| `forge_engine` | platform/window, rhi/*, render/*, frontend/*, ui/*, audio | SDL3, camera, assets, anim, world, imgui |

Кастомные цели:

| Таргет | Делает |
|---|---|
| `forge_shaders` | `cook_shaders.py --check` — cooked шейдеры на месте |
| `forge_stage_runtime` | копирует `assets/` + `shaders/cooked` в `build/` |

Макрос `forge_lab(target …)`: executable, линк `forge_engine`, `-Wall -Wextra -Wpedantic`, зависимость от stage.

Опции:

- `FORGE_DEV_UI` (default ON) — ImGui оверлей
- `FORGE_GPU_TESTS` (default OFF) — GPU smoke в CTest

Пресеты: `macos-debug` (Ninja, `build/`), `macos-release` (RelWithDebInfo, `build-release/`, `FORGE_DEV_UI=OFF`).

---

## Бинарники

Старые лабы **не удаляются**. `forge` всегда = последняя.

| Бинарь | Источник | Что показывает |
|---|---|---|
| `forge_m1` | `tests/m1/` | камера, depth, unlit cubes |
| `forge_m2` | `tests/m2/` | glTF PBR + IBL + ACES (FlightHelmet) |
| `forge_m3` | `tests/m3/` | CSM + Jolt capsule |
| `forge_m4` | `tests/m4/` | сектора, instancing, frustum |
| `forge_m5` | `tests/m5/` | скин (Fox) + машина |
| `forge_m6` | `tests/m6/` | Lua API |
| `forge_m7` | `tests/m7/` | UDP клиент-сервер, Lua net.lua |
| `forge` / `forge_survival` | `tests/survival/` | survival-сессия: меню, инвентарь, хост/джойн |

Каждая лаба: `main.cpp` (точка входа) + `lab.cpp` / `lab.hpp` (`app::Lab`). Survival дополнительно `shell.cpp` — меню и `app::run_game`.

Запуск:

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
./build/forge
```

Smoke без окна-цикла навсегда: `FORGE_SMOKE=<frames> ./build/forge`.

---

## Шейдеры (`shaders/`)

Исходники — HLSL. `scripts/cook_shaders.py` → `shaders/cooked/*.{msl,spv,dxil,json}`. На рантайме `rhi::shader` грузит cooked под текущий бэкенд. Stage копирует cooked в `build/shaders/`.

| Шейдер | Роль |
|---|---|
| `pbr.vert` / `pbr.frag` | forward PBR |
| `pbr_instanced.vert` | инстансы пропов |
| `pbr_skinned.vert` | скелет, palette костей |
| `shadow.vert` / `shadow.frag` | CSM depth |
| `tonemap.vert` / `tonemap.frag` | ACES на HDR |
| `bloom.frag` | bloom |
| `unlit.vert` / `unlit.frag` | без освещения (M1) |
| `ui.vert` / `ui.frag` | SDF UI / текст |

Не добавлять второй GPU-бэкенд руками. SDL3 GPU — единственный RHI. M0 = MSL; позже shadercross компилирует HLSL → MSL / SPIR-V / DXIL.

---

## Ассеты (`assets/`)

CMake staging копирует это в `build/assets/` (и Lua в `build/scripts/`).

| Путь | Содержимое |
|---|---|
| `models/Als/Mannequin.glb` | Mixamo X Bot — игрок survival |
| `models/Fox/Fox.glb` | glTF Fox, лаба скинов M5 |
| `models/FlightHelmet/` | PBR-тест Khronos (glTF + KTX2) |
| `models/Nature/` | Kenney деревья / камни |
| `models/SurvivalKit/` | Kenney инструменты, ресурсы, бочка, верстак |
| `models/Campfire/campfire-pit.glb` | костёр |
| `effects/Fire/flame_01.png` | билборд огня |
| `fonts/DroidSans.ttf` | MSDF UI |
| `menu/background.png` | заставка меню |
| `scripts/main.lua` | Lua survival / лаборатории |
| `scripts/net.lua` | Lua-сеть (M7) |

У каждой папки моделей есть `SOURCE.txt` / `LICENSE*`.

---

## Тесты (`tests/`)

Юниты линкуют узкую либу, не весь `forge_engine`, где это возможно.

| Файл | Что проверяет | Линк |
|---|---|---|
| `camera_tests.cpp` | камера | `forge_camera` |
| `gltf_tests.cpp` | загрузка glTF | `forge_assets` |
| `world_tests.cpp` | сектора / cull | `forge_world` |
| `skin_tests.cpp` | скины и клипы | `forge_assets` |
| `script_tests.cpp` | Lua | `forge_script` |
| `phys_tests.cpp` | Jolt + ALS (прыжок, стрейф, mantle) | `forge_physics` |
| `net_tests.cpp` | пакеты, handshake, stream, ACK | `forge_game_net` |
| `survival_tests.cpp` | сессия: добыча, extract, мили, респаун | `forge_game_net` |
| `economy_tests.cpp` | крафт, инструменты, станции | `forge_game` |
| `ui_tests.cpp` | UI-кит / шрифт | `forge_engine` |
| `app_tests.cpp` | меню / настройки | `forge_engine` + `forge_game_net` |
| `settings_tests.cpp` | persist настроек | `forge_engine` |
| `shader_assets_test.py` | cooked шейдеры / негативные кейсы | Python |
| `arch_tests.cpp` | FrameGraph compile, jobs, prediction, save, residency, packet budget | engine + game_net |
| `fixtures/*.gltf` | крошечные glTF для юнитов | — |

CTest ещё гоняет `cook_shaders.py --check` и `cook_textures.py --check`. GPU smoke (`scripts/smoke.py` на каждый `forge_mN`) — только если `FORGE_GPU_TESTS=ON`.

---

## Скрипты (`scripts/`)

| Скрипт | Делает |
|---|---|
| `progress.py` | список / `close <id>` роадмапа (TOML + чекбоксы README) |
| `cook_shaders.py` | HLSL → MSL / SPIR-V / DXIL, `--check` |
| `cook_textures.py` | PNG → KTX2, `--check` |
| `build_shadercross.py` | сборка sdl-shadercross |
| `smoke.py` | GPU smoke: `FORGE_SMOKE` N кадров |
| `normalize_nature.py` | правка Nature-кита (масштаб / ориентация) |

---

## `third_party/`

Не FetchContent — лежит в репо.

| Путь | Зачем |
|---|---|
| `cgltf/cgltf.h` | парсер glTF |
| `mikktspace/` | касательные |
| `stb/stb_image.h` | картинки |
| `stb/stb_rect_pack.h` | упаковка атласа |
| `stb/stb_truetype.h` | TTF → MSDF |

Jolt, glm, KTX, Lua, ImGui качаются CMake в `build/_deps/`.

---

## Что сознательно не здесь

- Второй GPU-бэкенд руками (только SDL3 GPU).
- Свой парсер glTF / своя физика / свой audio mixer (когда дойдём — miniaudio/SDL audio).
- Clustered PBR / Nanite / Lumen. M2 = forward PBR + IBL + ACES, HDR offscreen обязателен.
- Дерево `GameObject` как модель мира.
- Игровая логика внутри `engine/net/` (транспорт чистый).
- Транспорт внутри `engine/game/` (игра не открывает сокеты напрямую).
