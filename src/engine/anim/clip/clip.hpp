#pragma once

#include "engine/anim/skeleton/animator.hpp"

namespace forge::anim {

inline int clip_index(const assets::Scene& scene, std::string_view name) { return find_clip(scene, name); }

} // namespace forge::anim
