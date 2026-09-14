#pragma once

#include "engine/ui/widgets/ui.hpp"

namespace forge::ui {

inline Rect pad(Rect rect, float amount)
{
    return {rect.x + amount, rect.y + amount, std::max(0.0f, rect.w - amount * 2),
            std::max(0.0f, rect.h - amount * 2)};
}

} // namespace forge::ui
