#pragma once

#include "engine/assets/manager/manager.hpp"

namespace forge::assets {

inline Manager& cache() { return catalog(); }

} // namespace forge::assets
