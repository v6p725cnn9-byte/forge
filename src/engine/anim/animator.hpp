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

} // namespace forge::anim
