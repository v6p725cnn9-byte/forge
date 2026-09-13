#include "engine/world/frustum.hpp"

namespace forge::world {

Frustum frustum_from_clip(const glm::mat4& clip)
{
    const auto row = [&](int index) {
        return glm::vec4{clip[0][index], clip[1][index], clip[2][index], clip[3][index]};
    };
    const glm::vec4 r0 = row(0);
    const glm::vec4 r1 = row(1);
    const glm::vec4 r2 = row(2);
    const glm::vec4 r3 = row(3);
    Frustum frustum;
    frustum.planes[0] = r3 + r0;
    frustum.planes[1] = r3 - r0;
    frustum.planes[2] = r3 + r1;
    frustum.planes[3] = r3 - r1;
    frustum.planes[4] = r2;
    frustum.planes[5] = r3 - r2;
    for (auto& plane : frustum.planes) {
        const float length = glm::length(glm::vec3{plane});
        if (length > 1e-8f) plane /= length;
    }
    return frustum;
}

bool aabb_visible(const Frustum& frustum, const Aabb& aabb)
{
    for (const auto& plane : frustum.planes) {
        const glm::vec3 positive{
            plane.x >= 0.0f ? aabb.max.x : aabb.min.x,
            plane.y >= 0.0f ? aabb.max.y : aabb.min.y,
            plane.z >= 0.0f ? aabb.max.z : aabb.min.z,
        };
        if (glm::dot(glm::vec3{plane}, positive) + plane.w < 0.0f) return false;
    }
    return true;
}

} // namespace forge::world
