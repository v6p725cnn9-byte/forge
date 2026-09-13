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

std::string fixture(const char* name)
{
    const char* root = std::getenv("FORGE_TEST_ROOT");
    check(root != nullptr, "FORGE_TEST_ROOT must be set");
    return std::string(root) + "/tests/fixtures/" + name;
}

std::string asset(const char* relative)
{
    const char* root = std::getenv("FORGE_TEST_ROOT");
    check(root != nullptr, "FORGE_TEST_ROOT must be set");
    return std::string(root) + "/" + relative;
}
}

int main()
{
    forge::assets::Scene scene;
    std::string error;

    check(forge::assets::load_gltf(fixture("triangle.gltf"), scene, error), error.c_str());
    check(scene.indices.size() == 3, "Triangle must have three indices");
    check(scene.vertices.size() == 3, "Welded triangle must keep three vertices");
    check(scene.primitives.size() == 1, "Triangle must produce one primitive");
    check(scene.materials[0].name == "Default", "Named material must load");
    check(std::abs(scene.materials[0].metallic - 0.0f) < 1e-5f, "Metallic factor must load");
    check(std::abs(scene.materials[0].roughness - 0.5f) < 1e-5f, "Roughness factor must load");
    check(scene.vertices[0].normal.y > 0.5f || scene.vertices[0].normal.z != 0.0f
              || glm::length(scene.vertices[0].normal) > 0.5f,
          "Missing normals must be generated");

    forge::assets::Scene rejected = scene;
    check(!forge::assets::load_gltf(fixture("blend.gltf"), rejected, error), "BLEND must fail");
    check(error.find("BLEND") != std::string::npos, "BLEND error must mention alpha mode");
    check(rejected.vertices.size() == scene.vertices.size(), "Failed load must leave destination intact");

    check(!forge::assets::load_gltf("/no/such/model.gltf", scene, error), "Missing file must fail");

    check(forge::assets::load_gltf(asset("assets/models/FlightHelmet/FlightHelmet.gltf"), scene, error),
          error.c_str());
    check(scene.primitives.size() >= 5, "Flight Helmet must load every mesh primitive");
    check(scene.vertices.size() > 1000 && scene.indices.size() > 1000, "Helmet geometry is unexpectedly small");
    check(scene.materials.size() >= 6, "Helmet materials must load");
    bool ktx2 = false;
    for (const auto& image : scene.images) {
        if (image.path.extension() == ".ktx2") ktx2 = true;
    }
    check(ktx2, "Helmet textures must prefer cooked KTX2");
    check(std::isfinite(scene.bounds_min.x) && std::isfinite(scene.bounds_min.y)
              && std::isfinite(scene.bounds_min.z) && std::isfinite(scene.bounds_max.x)
              && std::isfinite(scene.bounds_max.y) && std::isfinite(scene.bounds_max.z),
          "Bounds must be finite");
    check(glm::length(scene.bounds_max - scene.bounds_min) > 0.05f, "Helmet bounds must be non-zero");
    bool warned_transmission = false;
    for (const auto& warning : scene.warnings) {
        if (warning.find("transmission") != std::string::npos) warned_transmission = true;
    }
    check(warned_transmission, "Optional transmission must warn, not fail");

    const auto cube = forge::assets::make_unit_cube();
    check(cube.vertices.size() == 24 && cube.indices.size() == 36, "Unit cube must be 24 verts / 36 indices");
    check(cube.primitives.size() == 1 && cube.materials.size() == 1, "Unit cube is one PBR primitive");
    check(cube.vertices[0].normal.z > 0.5f, "First cube face faces +Z");

    std::cout << "glTF checks passed (" << scene.vertices.size() << " vertices, "
              << scene.primitives.size() << " primitives)\n";
}
