#pragma once

#include "engine/anim/skeleton/animator.hpp"

namespace forge::anim {

inline glm::mat4 hide_joint(const glm::mat4& skinning, const glm::mat4& inverse_bind)
{
    return collapse_joint(skinning, inverse_bind);
}

} // namespace forge::anim
