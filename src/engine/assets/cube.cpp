#include "engine/assets/scene.hpp"

namespace forge::assets {
namespace {

void push_face(Scene& scene, const glm::vec3& n, const glm::vec3& t, const glm::vec3 p[4])
{
    const auto base = static_cast<std::uint32_t>(scene.vertices.size());
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = p[i];
        vertex.normal = n;
        vertex.tangent = glm::vec4{t, 1.0f};
        vertex.uv0 = uv[i];
        vertex.uv1 = uv[i];
        vertex.color = glm::vec4{1.0f};
        scene.vertices.push_back(vertex);
    }
    scene.indices.insert(scene.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

Scene make_unit_cube()
{
    Scene scene;
    const glm::vec3 p[6][4] = {
        {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
        {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},
        {{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
        {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}},
        {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}},
        {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}},
    };
    const glm::vec3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const glm::vec3 t[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
    for (int face = 0; face < 6; ++face) push_face(scene, n[face], t[face], p[face]);

    Material material;
    material.name = "Cube";
    material.metallic = 0.04f;
    material.roughness = 0.48f;
    scene.materials.push_back(material);
    scene.primitives.push_back({0, static_cast<std::uint32_t>(scene.indices.size()), 0});
    scene.bounds_min = {-0.5f, -0.5f, -0.5f};
    scene.bounds_max = {0.5f, 0.5f, 0.5f};
    return scene;
}

} // namespace forge::assets
