#include "engine/core/camera.hpp"

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
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
}

int main()
{
    forge::Camera camera;
    const auto eye = camera.view() * glm::vec4(camera.position, 1.0f);
    check(glm::length(glm::vec3(eye)) < 0.0001f, "View must transform camera position to origin");
    const auto proj = camera.projection(16.0f / 9.0f);
    const auto n = proj * glm::vec4(0, 0, -camera.near_plane, 1);
    const auto f = proj * glm::vec4(0, 0, -camera.far_plane, 1);
    check(near(n.z / n.w, 0) && near(f.z / f.w, 1), "SDL clip depth must be [0,1]");
    const auto front = camera.view() * glm::vec4(camera.position + camera.forward(), 1);
    check(near(front.z, -1), "Camera must face negative view Z");

    camera.yaw = -90;
    camera.pitch = 0;
    camera.position = {};
    camera.move({1, 0, 1}, 1.0f, false);
    check(near(glm::length(camera.position), camera.speed), "Diagonal movement must be normalized");
    camera.position = {};
    camera.move({0, 0, 1}, 1.0f, false);
    const auto one_step = camera.position;
    camera.position = {};
    for (int i = 0; i < 100; ++i) camera.move({0, 0, 1}, 0.01f, false);
    check(glm::length(camera.position - one_step) < 0.0001f, "Movement must be frame-rate independent");
    camera.position = {};
    camera.move({0, 0, 1}, 1.0f, true);
    check(near(glm::length(camera.position), camera.speed * 4), "Boost must multiply movement by four");
    camera.position = {};
    camera.pitch = 60;
    camera.move({0, 1, 1}, 1.0f, false);
    check(near(glm::length(camera.position), camera.speed), "Vertical + pitched movement must keep the same speed");
    camera.position = {0, 0, 4};
    camera.look_at({0, 0, 0});
    check(near(camera.pitch, 0) && near(camera.yaw, -90), "look_at must face the target along -Z");
    const auto before = camera.view() * glm::vec4(0, 0, 0, 1);
    check(before.z < 0, "Framed target must sit in front of the camera");
    camera.frame({-1, -1, -1}, {1, 1, 1});
    check(glm::length(camera.position) > 1.0f, "frame() must back the camera off the bounds");
    camera.look(1000000, -1000000);
    check(camera.pitch == 89 && std::abs(camera.yaw) <= 180, "Mouse look must clamp pitch and wrap yaw");
    const auto view = camera.view();
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        check(std::isfinite(view[c][r]), "View must stay finite at pitch limit");
    std::cout << "Camera checks passed\n";
}
