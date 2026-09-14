#include "engine/app/lab.hpp"
#include "engine/net/protocol.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

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
    // Every camera yaw crossed with every WASD combo, diagonals included.
    for (int deg = 0; deg < 360; deg += 15) {
        const float t = glm::radians(static_cast<float>(deg));
        const glm::vec3 flat{std::cos(t), 0.0f, std::sin(t)};
        for (float sx = -1.0f; sx <= 1.0f; sx += 1.0f) {
            for (float pz = -1.0f; pz <= 1.0f; pz += 1.0f) {
                const glm::vec3 walk = forge::app::compose_walk(flat, sx, pz);
                check(std::abs(walk.x) <= 1.0f && std::abs(walk.z) <= 1.0f, "Walk must fit the input range");
                const float length = glm::length(glm::vec2(walk.x, walk.z));
                check(length <= 1.0f + 1e-4f, "Walk must never exceed unit length");
                if (sx != 0.0f && pz != 0.0f)
                    check(length > 0.999f, "Full diagonal must keep full speed");
                if (sx == 0.0f && pz == 0.0f) check(length == 0.0f, "No input must not move");
                // The composed vector is what goes on the wire: it must survive validation.
                forge::net::Input wire{9, walk.x, walk.z, 0.0f, false};
                const auto bytes = forge::net::pack_input(wire);
                forge::net::Input back{};
                check(forge::net::unpack_input(bytes.data(), bytes.size(), back), "Diagonal must pass input validation");
            }
        }
    }

    // Directions: W follows the view, D strafes right of it.
    const glm::vec3 flat{0.0f, 0.0f, -1.0f};
    const glm::vec3 w = forge::app::compose_walk(flat, 0.0f, 1.0f);
    check(glm::length(w - flat) < 1e-5f, "W must walk along the view");
    const glm::vec3 d = forge::app::compose_walk(flat, 1.0f, 0.0f);
    check(glm::length(d - glm::vec3{1.0f, 0.0f, 0.0f}) < 1e-5f, "D must strafe right of the view");
    const glm::vec3 s = forge::app::compose_walk(flat, 0.0f, -1.0f);
    check(glm::length(s + flat) < 1e-5f, "S must backpedal against the view");
    check(glm::length(forge::app::compose_walk(glm::vec3{0}, 1.0f, 1.0f)) == 0.0f, "Degenerate view must not move");

    std::cout << "App checks passed\n";
}
