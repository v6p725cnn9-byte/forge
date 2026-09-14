#include "engine/anim/animator.hpp"
#include "engine/assets/scene.hpp"

#include <cmath>
#include <cstdlib>
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
}

int main()
{
    const char* root = std::getenv("FORGE_TEST_ROOT");
    check(root != nullptr, "FORGE_TEST_ROOT must be set");
    forge::assets::Scene scene;
    std::string error;
    check(forge::assets::load_gltf(std::string(root) + "/tests/fixtures/simple_skin.gltf", scene, error),
          error.c_str());
    check(scene.skins.size() == 1 && scene.skins[0].joints.size() == 2, "Fixture must have a 2-joint skin");
    check(scene.animations.size() == 1 && scene.animations[0].name == "bend", "Fixture must expose the bend clip");
    check(scene.primitives.size() == 1 && scene.primitives[0].skin == 0, "Skinned primitive must record its skin");
    check(scene.vertices.size() >= 3, "Skinned mesh must have vertices");
    check(scene.vertices[0].weights.x > 0.5f, "Bottom vertices bind to joint 0");

    forge::anim::Palette rest{};
    forge::anim::Palette bent{};
    check(forge::anim::evaluate(scene, 0, 0, 0.0f, rest), "Rest palette");
    check(forge::anim::evaluate(scene, 0, 0, 0.5f, bent), "Bent palette");
    check(rest.count == 2 && bent.count == 2, "Palette size follows the skin");
    const float delta = glm::length(glm::vec3(bent.joints[1][3] - rest.joints[1][3]))
        + glm::length(glm::vec3(bent.joints[1][0] - rest.joints[1][0]));
    check(delta > 0.1f, "Animating a joint must change its palette matrix");
    check(forge::anim::find_clip(scene, "bend") == 0, "find_clip must match the clip name");

    forge::assets::Scene fox;
    check(forge::assets::load_gltf(std::string(root) + "/assets/models/Fox/Fox.glb", fox, error), error.c_str());
    check(fox.skins.size() == 1 && fox.skins[0].joints.size() == 24, "Fox must load 24 joints");
    check(fox.animations.size() == 3, "Fox has Survey/Walk/Run");
    check(forge::anim::find_clip(fox, "Walk") >= 0 && forge::anim::find_clip(fox, "Run") >= 0, "Named Fox clips");
    forge::anim::Palette fox_palette{};
    check(forge::anim::evaluate(fox, 0, forge::anim::find_clip(fox, "Walk"), 0.3f, fox_palette), "Fox walk palette");
    check(fox_palette.count == 24, "Fox palette uses every joint");

    const int head_like = 1;
    const glm::mat4 collapsed =
        forge::anim::collapse_joint(fox_palette.joints[head_like], fox.skins[0].inverse_bind[head_like]);
    const glm::vec4 at_a = collapsed * glm::vec4{1.0f, 2.0f, 3.0f, 1.0f};
    const glm::vec4 at_b = collapsed * glm::vec4{-4.0f, 0.5f, 8.0f, 1.0f};
    check(std::abs(at_a.w - 1.0f) < 1e-5f, "Collapsed joint keeps w=1");
    check(glm::length(glm::vec3(at_a - at_b)) < 1e-4f, "Collapsed joint maps every vertex to one point");
    const glm::vec4 joint_center = fox_palette.joints[head_like] * glm::inverse(fox.skins[0].inverse_bind[head_like])
        * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
    check(glm::length(glm::vec3(at_a) - glm::vec3(joint_center) / joint_center.w) < 1e-3f,
          "Collapsed point must sit at the joint center");
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) check(std::isfinite(collapsed[c][r]), "Collapsed matrix must stay finite");

    std::cout << "Skin checks passed (" << fox.vertices.size() << " fox verts, " << fox.animations.size()
              << " clips)\n";
}
