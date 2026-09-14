#pragma once

#include "engine/rhi/command/command.hpp"

namespace forge::render {

// Alpha/particle pass slot. Survival fire billboards currently draw in opaque
// with alpha mask; this pass exists so FrameGraph can order it after opaque.
inline bool draw_transparent(rhi::Command&) { return true; }

} // namespace forge::render
