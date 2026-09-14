#include "engine/render/survival_visuals.hpp"

#include "engine/core/paths.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <string>

namespace forge::render {
namespace {

constexpr float kBlendSeconds = 0.18f;

glm::mat4 grounded(const PbrScene& model, float height)
{
    const float scale = height / std::max(model.bounds_max.y - model.bounds_min.y, 0.001f);
    const glm::vec3 base{(model.bounds_min.x + model.bounds_max.x) * 0.5f, model.bounds_min.y,
                         (model.bounds_min.z + model.bounds_max.z) * 0.5f};
    return glm::scale(glm::mat4(1), glm::vec3{scale}) * glm::translate(glm::mat4(1), -base);
}

bool load_prop(PbrScene& gpu, SDL_GPUDevice* device, const std::filesystem::path& path, std::string& error, std::shared_ptr<Ibl> lighting)
{
    assets::Scene scene;
    if (!assets::load_gltf(path, scene, error)) return false;
    // The older Nature Kit exports wood and foliage with metallic=1.
    for (auto& material : scene.materials) material.metallic = 0;
    return gpu.ingest(device, std::move(scene), path.stem().string(), error, std::move(lighting));
}

assets::Scene fire_billboard()
{
    assets::Scene scene;
    assets::Material material;
    material.name = "Kenney flame_01";
    material.base_color_factor = {1.0f, 0.32f, 0.025f, 1.0f};
    material.emissive_factor = {3.0f, 0.65f, 0.03f};
    material.metallic = 0;
    material.unlit = true;
    material.alpha_mask = true;
    material.alpha_cutoff = 0.16f;
    material.double_sided = true;
    material.textures[assets::base_color].image = 0;
    material.textures[assets::emissive].image = 0;
    material.textures[assets::base_color].wrap_s = material.textures[assets::base_color].wrap_t = 33071;
    material.textures[assets::emissive].wrap_s = material.textures[assets::emissive].wrap_t = 33071;
    scene.materials.push_back(material);
    scene.images.push_back({assets_directory() / "effects/Fire/flame_01.png", {}});
    // Geometry only carries the downloaded particle artwork; no authored flame mesh.
    for (const auto& corner : std::array<glm::vec4, 4>{{{-0.5f, 0, 0, 1}, {0.5f, 0, 1, 1},
                                                       {0.5f, 1, 1, 0}, {-0.5f, 1, 0, 0}}}) {
        assets::Vertex vertex;
        vertex.position = {corner.x, corner.y, 0};
        vertex.normal = {0, 0, 1};
        vertex.uv0 = {corner.z, corner.w};
        scene.vertices.push_back(vertex);
    }
    scene.indices = {0, 1, 2, 0, 2, 3};
    scene.primitives.push_back({0, 6, 0, -1});
    scene.bounds_min = {-0.5f, 0, 0};
    scene.bounds_max = {0.5f, 1, 0};
    return scene;
}

std::string lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string joint_leaf(std::string_view name)
{
    const auto colon = name.find_last_of(":/");
    return lower(colon == std::string_view::npos ? name : name.substr(colon + 1));
}

int named_clip(const assets::Scene& scene, const char* name)
{
    const std::string want = lower(name);
    for (std::size_t i = 0; i < scene.animations.size(); ++i)
        if (lower(scene.animations[i].name) == want) return static_cast<int>(i);
    return -1;
}

int named_joint(const assets::Scene& scene, const assets::Skin& skin, std::initializer_list<const char*> names)
{
    for (const char* name : names) {
        const std::string want = lower(name);
        for (std::size_t j = 0; j < skin.joints.size(); ++j) {
            const int node = skin.joints[j];
            if (node < 0 || node >= static_cast<int>(scene.nodes.size())) continue;
            if (joint_leaf(scene.nodes[static_cast<std::size_t>(node)].name) == want)
                return static_cast<int>(j);
        }
    }
    return -1;
}

} // namespace

bool SurvivalVisuals::create(rhi::Host& host)
{
    std::string error;
    const auto models = assets_directory() / "models";
    assets::Scene source;
    if (!assets::load_gltf(models / "Als/Mannequin.glb", source, error)) {
        SDL_Log("ALS mannequin load failed: %s", error.c_str());
        return false;
    }
    const int source_walk = named_clip(source, "walk");
    anim::Palette initial_pose;
    glm::vec3 posed_min{0}, posed_max{0};
    bool have_bounds = false;
    if (!source.skins.empty() && anim::evaluate(source, 0, source_walk, 0, initial_pose)) {
        const auto& skin = source.skins[0];
        hip_joint_ = named_joint(source, skin, {"Hips", "Hip", "pelvis"});
        head_joint_ = named_joint(source, skin, {"Head"});
        if (hip_joint_ >= 0) {
            hip_bind_ = glm::inverse(skin.inverse_bind[static_cast<std::size_t>(hip_joint_)]);
            hip_origin_ = glm::vec3(initial_pose.joints[hip_joint_] * hip_bind_ * glm::vec4{0, 0, 0, 1});
        }
        // Bounds must be measured after skinning: raw mesh coordinates are not the posed character.
        for (const auto& vertex : source.vertices) {
            glm::vec4 point{0};
            float weight = 0;
            for (int j = 0; j < 4; ++j) {
                point += vertex.weights[j] * (initial_pose.joints[static_cast<int>(vertex.joints[j])]
                    * glm::vec4(vertex.position, 1));
                weight += vertex.weights[j];
            }
            if (weight < 0.00001f) point = glm::vec4(vertex.position, 1);
            const glm::vec3 position = glm::vec3(point);
            if (!have_bounds) { posed_min = posed_max = position; have_bounds = true; }
            posed_min = glm::min(posed_min, position);
            posed_max = glm::max(posed_max, position);
        }
    }
    if (!character_.ingest(host.device(), std::move(source), "ALS mannequin", error)) {
        SDL_Log("ALS mannequin upload failed: %s", error.c_str());
        return false;
    }
    const auto& character = character_.cpu();
    idle_ = named_clip(character, "idle");
    walk_ = named_clip(character, "walk");
    run_ = named_clip(character, "run");
    if (run_ < 0) run_ = walk_;
    interact_ = named_clip(character, "interact");
    character_transform_ = grounded(character_, 1.80f);
    if (have_bounds) {
        const float scale = 1.80f / std::max(posed_max.y - posed_min.y, 0.001f);
        const glm::vec3 base{(posed_min.x + posed_max.x) * 0.5f, posed_min.y,
                             (posed_min.z + posed_max.z) * 0.5f};
        character_transform_ = glm::scale(glm::mat4(1), glm::vec3{scale}) * glm::translate(glm::mat4(1), -base);
    }
    if (!character.skins.empty() && hip_joint_ >= 0
        && hip_joint_ < static_cast<int>(character.skins[0].joints.size())) {
        const int hip_node = character.skins[0].joints[static_cast<std::size_t>(hip_joint_)];
        const float model_scale = glm::length(glm::vec3(character_transform_[0]));
        if (walk_ >= 0) walk_stride_ = anim::stride_speed(character, hip_node, walk_) * model_scale;
        if (run_ >= 0) run_stride_ = anim::stride_speed(character, hip_node, run_) * model_scale;
        SDL_Log("Locomotion stride: walk %.2f m/s, run %.2f m/s", walk_stride_, run_stride_);
    }

    const std::array<const char*, 3> trees{"tree_default.glb", "tree_pineTallA.glb", "tree_pineRoundC.glb"};
    for (std::size_t i = 0; i < trees.size(); ++i) {
        if (!load_prop(trees_[i], host.device(), models / "Nature" / trees[i], error, character_.lighting())) {
            SDL_Log("Tree load failed: %s", error.c_str());
            return false;
        }
        tree_transforms_[i] = grounded(trees_[i], i == 1 ? 6.2f : 4.8f);
    }
    if (!load_prop(rock_, host.device(), models / "Nature/rock_largeA.glb", error, character_.lighting())
        || !load_prop(campfire_, host.device(), models / "Campfire/campfire-pit.glb", error, character_.lighting())
        || !flame_.ingest(host.device(), fire_billboard(), "Kenney fire particle", error, character_.lighting())) {
        SDL_Log("Survival prop load failed: %s", error.c_str());
        return false;
    }
    const std::array<const char*,10> supplies{{"tool-axe", "tool-pickaxe", "tool-axe-upgraded", "tool-pickaxe-upgraded",
        "resource-wood", "resource-stone", "resource-stone-large", "grass", "workbench", "barrel-open"}};
    const std::array<float,10> heights{{.75f,.85f,.75f,.85f,.22f,.20f,1.15f,.45f,1.0f,1.15f}};
    for (std::size_t i = 0; i < supplies.size(); ++i) {
        if (!load_prop(supplies_[i],host.device(),models / "SurvivalKit" / (std::string(supplies[i])+".glb"),error,character_.lighting())) {
            SDL_Log("Supply model failed: %s",error.c_str());
            return false;
        }
        supply_transforms_[i] = grounded(supplies_[i],heights[i]);
    }
    rock_transform_ = grounded(rock_, 0.75f);
    fire_transform_ = grounded(campfire_, 0.42f);
    pbr_ = make_pbr_pipeline(host, false);
    double_sided_ = make_pbr_pipeline(host, true);
    skinned_ = make_pbr_skinned_pipeline(host, true);
    if (!pbr_ || !double_sided_ || !skinned_) return false;
    SDL_Log("Survival assets ready: ALS mannequin (%zu skins, %zu clips), trees, campfire and rock",
            character.skins.size(), character.animations.size());
    return true;
}

void SurvivalVisuals::remove_root_motion(anim::Palette& palette) const
{
    if (hip_joint_ < 0 || hip_joint_ >= palette.count) return;
    const glm::vec3 hip = glm::vec3(palette.joints[hip_joint_] * hip_bind_ * glm::vec4{0, 0, 0, 1});
    // Movement belongs to the network simulation. Preserve the authored vertical bob.
    const auto correction = glm::translate(glm::mat4(1), glm::vec3{hip_origin_.x - hip.x, 0, hip_origin_.z - hip.z});
    for (int j = 0; j < palette.count; ++j) palette.joints[j] = correction * palette.joints[j];
}

void SurvivalVisuals::animate(PlayerVisual& player, float dt)
{
    const auto& scene = character_.cpu();
    if (scene.skins.empty()) return;
    if (scene.animations.empty()) {
        anim::evaluate(scene, 0, -1, 0.0f, player.palette);
        remove_root_motion(player.palette);
        return;
    }
    const int next = player.action_left > 0 ? interact_
        : (player.speed > 3.85f ? run_ : (player.speed > 0.50f ? walk_ : idle_));
    if (next < 0) {
        // Until an idle clip is supplied, hold the first authored walking pose.
        anim::evaluate(scene, 0, walk_, 0.0f, player.palette);
        remove_root_motion(player.palette);
        player.clip = -1;
        player.time = 0;
        return;
    }
    if (next != player.clip) {
        player.previous_clip = player.clip;
        player.previous_time = player.time;
        player.clip = next;
        player.time = 0;
        player.blend = player.previous_clip < 0 ? 1.0f : 0.0f;
    }
    // ALS-style stride matching: run the cycle faster/slower so the feet
    // land where the capsule travels instead of sliding.
    float stride = 0.0f;
    if (next == walk_) stride = walk_stride_;
    else if (next == run_) stride = run_stride_;
    float rate = 1.0f;
    if (stride > 0.2f && player.speed > 0.3f) rate = std::clamp(player.speed / stride, 0.5f, 2.5f);
    player.time += dt * rate;
    player.previous_time += dt * rate;
    player.blend = std::min(1.0f, player.blend + dt / kBlendSeconds);
    std::vector<glm::vec3> translation, scale;
    std::vector<glm::quat> rotation;
    anim::sample_clip(scene, player.clip, player.time, translation, rotation, scale);
    if (player.blend < 1.0f) {
        std::vector<glm::vec3> old_translation, old_scale;
        std::vector<glm::quat> old_rotation;
        anim::sample_clip(scene, player.previous_clip, player.previous_time, old_translation, old_rotation, old_scale);
        const float blend = player.blend * player.blend * (3.0f - 2.0f * player.blend);
        for (std::size_t i = 0; i < translation.size(); ++i) {
            translation[i] = glm::mix(old_translation[i], translation[i], blend);
            scale[i] = glm::mix(old_scale[i], scale[i], blend);
            if (glm::dot(old_rotation[i], rotation[i]) < 0) rotation[i] = -rotation[i];
            rotation[i] = glm::normalize(glm::slerp(old_rotation[i], rotation[i], blend));
        }
    }
    std::vector<glm::mat4> globals;
    anim::compute_globals(scene, translation, rotation, scale, globals);
    anim::compute_palette(scene, 0, globals, player.palette);
    remove_root_motion(player.palette);
}

void SurvivalVisuals::update(const net::Snapshot& snapshot, std::uint8_t local_player, float dt, bool interact)
{
    dt = std::clamp(dt, 0.0f, 0.1f);
    clock_ += dt;
    std::array<bool, 256> visible{};
    for (const auto& ghost : snapshot.entities) {
        if (ghost.kind != net::Kind::Player) continue;
        auto& player = players_[ghost.id];
        visible[ghost.id] = true;
        if (!player.active) {
            player = {};
            player.active = true;
            player.target = player.position = ghost.position;
            player.yaw = ghost.yaw;
            player.tick = snapshot.tick;
        } else if (net::sequence_newer(snapshot.tick, player.tick)) {
            const float seconds = static_cast<float>(snapshot.tick - player.tick) / static_cast<float>(net::kTickHz);
            const glm::vec3 raw =
                (ghost.position - player.target) / std::max(seconds, 0.001f);
            player.speed = glm::length(glm::vec2{raw.x, raw.z});
            const float blend = 1.0f - std::exp(-8.0f * std::max(dt, 0.001f));
            player.vel = glm::mix(player.vel, raw, blend);
            const float yaw_rad = glm::radians(player.yaw);
            const glm::vec2 fwd{std::sin(yaw_rad), std::cos(yaw_rad)};
            const glm::vec2 side{std::cos(yaw_rad), -std::sin(yaw_rad)};
            const glm::vec2 local{glm::dot(glm::vec2(player.vel.x, player.vel.z), fwd),
                                  glm::dot(glm::vec2(player.vel.x, player.vel.z), side)};
            player.lean = glm::mix(player.lean, glm::clamp(local * 0.035f, -0.18f, 0.18f), blend);
            player.target = ghost.position;
            player.tick = snapshot.tick;
        }
        const float smoothing = 1.0f - std::exp(-18.0f * dt);
        player.position = glm::mix(player.position, player.target, smoothing);
        player.yaw += std::remainder(ghost.yaw - player.yaw, 360.0f) * smoothing;
        player.action_left = std::max(0.0f, player.action_left - dt);
        if (interact_ >= 0 && interact && ghost.id == local_player && player.action_left <= 0) {
            player.action_left = character_.cpu().animations[static_cast<std::size_t>(interact_)].duration;
            if (player.clip == interact_) player.time = 0;
        }
        animate(player, dt);
    }
    for (std::size_t i = 0; i < players_.size(); ++i) if (!visible[i]) players_[i].active = false;
}

glm::vec3 SurvivalVisuals::player_position(std::uint8_t id, const glm::vec3& fallback) const
{
    return players_[id].active ? players_[id].position : fallback;
}

void SurvivalVisuals::draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, const net::Snapshot& snapshot,
                          const glm::mat4& view_projection, const DebugState& debug, const glm::vec3& camera_position,
                          int headless_player)
{
    triangles_ = 0;
    const auto draw = [&](const PbrScene& scene, const glm::mat4& transform, const glm::vec4& tint = glm::vec4{1}) {
        const CameraUniforms camera{view_projection, transform};
        SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
        scene.draw(command, pass, pbr_, double_sided_, debug, camera_position, {}, tint);
        triangles_ += scene.triangle_count;
    };
    for (const auto& ghost : snapshot.entities) {
        const auto position = glm::translate(glm::mat4(1), ghost.position);
        if (ghost.kind == net::Kind::Player) {
            const auto& player = players_[ghost.id];
            if (!player.active) continue;
            const auto& character = character_.cpu();
            const bool headless = static_cast<int>(ghost.id) == headless_player;
            if (headless && character.skins.empty()) continue;
            // The simulation stores the pawn center one metre above its feet.
            const auto model = glm::translate(glm::mat4(1), player.position - glm::vec3{0, 1, 0})
                * glm::rotate(glm::mat4(1), glm::radians(player.yaw), glm::vec3{0, 1, 0})
                * glm::rotate(glm::mat4(1), player.lean.x, glm::vec3{1, 0, 0})
                * glm::rotate(glm::mat4(1), player.lean.y, glm::vec3{0, 0, 1}) * character_transform_;
            const CameraUniforms camera{view_projection, model};
            SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
            if (character.skins.empty()) {
                character_.draw(command, pass, pbr_, double_sided_, debug, camera_position);
            } else if (headless && head_joint_ >= 0 && head_joint_ < player.palette.count
                       && head_joint_ < static_cast<int>(character.skins[0].inverse_bind.size())) {
                // CS2-style first person: collapse the head onto its own center so it
                // culls away, while legs, torso and arms stay visible below the eye.
                anim::Palette body = player.palette;
                const auto& skin = character.skins[0];
                body.joints[head_joint_] =
                    anim::collapse_joint(player.palette.joints[head_joint_], skin.inverse_bind[head_joint_]);
                character_.draw_skinned(command, pass, skinned_, body, debug, camera_position);
            } else {
                character_.draw_skinned(command, pass, skinned_, player.palette, debug, camera_position);
            }
            triangles_ += character_.triangle_count;
            int tool = -1;
            switch (ghost.equipped) {
            case game::Item::StoneAxe: tool = 0; break;
            case game::Item::StonePickaxe: tool = 1; break;
            case game::Item::IronAxe: tool = 2; break;
            case game::Item::IronPickaxe: tool = 3; break;
            default: break;
            }
            if (tool >= 0) {
                const auto turn = glm::rotate(glm::mat4(1),glm::radians(player.yaw),glm::vec3{0,1,0});
                const auto grip = glm::translate(glm::mat4(1),player.position) * turn
                    * glm::translate(glm::mat4(1),glm::vec3{.38f,-.15f,headless ? .75f : .28f});
                draw(supplies_[tool],grip * glm::rotate(glm::mat4(1),glm::radians(-20.0f),glm::vec3{1,0,0})
                    * supply_transforms_[tool]);
            }
        } else if (ghost.kind == net::Kind::Tree) {
            const auto variant = static_cast<std::size_t>(ghost.id % trees_.size());
            const float scale = 0.88f + static_cast<float>(ghost.id % 5) * 0.055f;
            draw(trees_[variant], position * glm::rotate(glm::mat4(1), glm::radians(ghost.id * 137.5f), glm::vec3{0, 1, 0})
                * glm::scale(glm::mat4(1), glm::vec3{scale}) * tree_transforms_[variant]);
        } else if (ghost.kind == net::Kind::Rock) {
            draw(rock_, position * glm::rotate(glm::mat4(1), glm::radians(ghost.id * 47.0f), glm::vec3{0, 1, 0})
                * rock_transform_, {0.62f, 0.57f, 0.50f, 1});
        } else if (ghost.kind >= net::Kind::Stick && ghost.kind <= net::Kind::Bench) {
            int prop = 5;
            glm::vec4 tint{1};
            switch (ghost.kind) {
            case net::Kind::Stick: prop = 4; break;
            case net::Kind::Pebble: prop = 5; break;
            case net::Kind::Flint: prop = 5; tint = {.36f,.45f,.56f,1}; break;
            case net::Kind::Fiber: prop = 7; break;
            case net::Kind::IronOre: prop = 6; tint = {.65f,.31f,.17f,1}; break;
            case net::Kind::Bench: prop = 8; break;
            case net::Kind::Furnace: prop = 9; tint = {.32f,.3f,.27f,1}; break;
            default: break;
            }
            draw(supplies_[prop],position * supply_transforms_[prop],tint);
        } else if (ghost.kind == net::Kind::Campfire) {
            draw(campfire_, position * fire_transform_);
            const auto to_camera = camera_position - ghost.position;
            const float facing = std::atan2(to_camera.x, to_camera.z);
            const float flicker = std::sin(clock_ * 8.0f + ghost.id) * 0.055f + std::sin(clock_ * 13.0f) * 0.025f;
            draw(flame_, glm::translate(glm::mat4(1), ghost.position + glm::vec3{0, 0.16f, 0})
                * glm::rotate(glm::mat4(1), facing, glm::vec3{0, 1, 0})
                * glm::scale(glm::mat4(1), glm::vec3{1.2f - flicker, 1.4f + flicker, 1}));
        }
    }
}

void SurvivalVisuals::destroy(rhi::Host& host)
{
    character_.destroy(host.device());
    for (auto& tree : trees_) tree.destroy(host.device());
    rock_.destroy(host.device());
    campfire_.destroy(host.device());
    flame_.destroy(host.device());
    for (auto& supply : supplies_) supply.destroy(host.device());
    if (pbr_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pbr_);
    if (double_sided_) SDL_ReleaseGPUGraphicsPipeline(host.device(), double_sided_);
    if (skinned_) SDL_ReleaseGPUGraphicsPipeline(host.device(), skinned_);
    pbr_ = double_sided_ = skinned_ = nullptr;
    for (auto& player : players_) player.active = false;
}

} // namespace forge::render
