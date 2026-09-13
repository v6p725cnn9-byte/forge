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

    std::cout << "Skin checks passed (" << fox.vertices.size() << " fox verts, " << fox.animations.size()
              << " clips)\n";
}
