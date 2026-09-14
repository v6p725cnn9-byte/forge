#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace forge::assets {

struct Vertex {
    glm::vec3 position{};
    glm::vec3 normal{0, 1, 0};
    glm::vec4 tangent{1, 0, 0, 1};
    glm::vec2 uv0{};
    glm::vec2 uv1{};
    glm::vec4 color{1};
    glm::vec4 joints{0};
    glm::vec4 weights{0};
};
static_assert(sizeof(Vertex) == 104);
static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, normal) == 12);
static_assert(offsetof(Vertex, tangent) == 24);
static_assert(offsetof(Vertex, uv0) == 40);
static_assert(offsetof(Vertex, uv1) == 48);
static_assert(offsetof(Vertex, color) == 56);
static_assert(offsetof(Vertex, joints) == 72);
static_assert(offsetof(Vertex, weights) == 88);

struct TextureTransform {
    glm::vec4 u{1, 0, 0, 0};
    glm::vec4 v{0, 1, 0, 0};
};

struct TextureRef {
    int image = -1;
    int min_filter = 9987;
    int mag_filter = 9729;
    int wrap_s = 10497;
    int wrap_t = 10497;
    TextureTransform transform;
};

enum TextureSlot { base_color, metallic_roughness, normal, occlusion, emissive, texture_slot_count };

struct Material {
    std::string name;
    glm::vec4 base_color_factor{1};
    glm::vec3 emissive_factor{0};
    float metallic = 1;
    float roughness = 1;
    float normal_scale = 1;
    float occlusion_strength = 1;
    float alpha_cutoff = 0.5f;
    bool alpha_mask = false;
    bool double_sided = false;
    bool unlit = false;
    std::array<TextureRef, texture_slot_count> textures;
};

struct ImageSource {
    std::filesystem::path path;
    std::vector<std::uint8_t> bytes;
};

struct Primitive {
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    std::uint32_t material = 0;
    int skin = -1;
};

constexpr int kMaxJoints = 64;

struct Node {
    std::string name;
    int parent = -1;
    glm::vec3 translation{0};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Skin {
    std::string name;
    std::vector<int> joints;
    std::vector<glm::mat4> inverse_bind;
};

enum class AnimPath { Translation, Rotation, Scale };
enum class AnimInterp { Linear, Step };

struct AnimChannel {
    int node = -1;
    AnimPath path = AnimPath::Translation;
    AnimInterp interp = AnimInterp::Linear;
    std::vector<float> times;
    std::vector<float> values;
};

struct Animation {
    std::string name;
    float duration = 0;
    std::vector<AnimChannel> channels;
};

struct Scene {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    std::vector<ImageSource> images;
    std::vector<Node> nodes;
    std::vector<Skin> skins;
    std::vector<Animation> animations;
    std::vector<std::string> warnings;
    glm::vec3 bounds_min{0};
    glm::vec3 bounds_max{0};
};

// A failed load leaves the destination intact.
bool load_gltf(const std::filesystem::path& path, Scene& destination, std::string& error);

Scene make_unit_cube();

// Procedural menu vista: camp knoll, lake basin, far ridge ring, still water
// plane and an unlit gradient sky dome with the sun glow baked toward
// sun_direction. Deterministic, no asset files.
Scene make_vista_terrain(const glm::vec3& sun_direction);

} // namespace forge::assets
