#pragma once

#include "engine/assets/scene.hpp"

#include <string_view>
#include <vector>

namespace forge::anim {

struct Palette {
    glm::mat4 joints[assets::kMaxJoints]{};
    int count = 0;
};

int find_clip(const assets::Scene& scene, std::string_view name);
void rest_pose(const assets::Scene& scene, std::vector<glm::vec3>& translation, std::vector<glm::quat>& rotation,
               std::vector<glm::vec3>& scale);
void sample_clip(const assets::Scene& scene, int clip, float time, std::vector<glm::vec3>& translation,
                 std::vector<glm::quat>& rotation, std::vector<glm::vec3>& scale);
void compute_globals(const assets::Scene& scene, const std::vector<glm::vec3>& translation,
                     const std::vector<glm::quat>& rotation, const std::vector<glm::vec3>& scale,
                     std::vector<glm::mat4>& globals);
bool compute_palette(const assets::Scene& scene, int skin, const std::vector<glm::mat4>& globals, Palette& out);
bool evaluate(const assets::Scene& scene, int skin, int clip, float time, Palette& out);
// Horizontal speed of a node over one clip cycle (stride length / duration),
// used to match playback rate to capsule speed so feet stop sliding.
// Returns 0 when the clip carries no root travel.
float stride_speed(const assets::Scene& scene, int node, int clip);
// Collapse a joint's skinning matrix onto the joint center so attached triangles
// degenerate and cull away. Used for a headless first-person body: legs and torso
// stay visible, the head no longer blocks the eye camera.
glm::mat4 collapse_joint(const glm::mat4& skinning, const glm::mat4& inverse_bind);

} // namespace forge::anim
