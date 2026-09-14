#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace forge::audio {

inline float attenuation(const glm::vec3& listener, const glm::vec3& source, float min_distance = 2.0f,
                         float max_distance = 40.0f)
{
    const float distance = glm::length(source - listener);
    if (distance <= min_distance) return 1.0f;
    if (distance >= max_distance) return 0.0f;
    return 1.0f - (distance - min_distance) / (max_distance - min_distance);
}

} // namespace forge::audio
