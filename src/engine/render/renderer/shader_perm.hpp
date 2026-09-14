#pragma once

#include <cstdint>

namespace forge::render {

enum class ShaderFeature : std::uint32_t {
    None = 0,
    Skinned = 1u << 0,
    Instanced = 1u << 1,
};

inline ShaderFeature operator|(ShaderFeature a, ShaderFeature b)
{
    return static_cast<ShaderFeature>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline bool has(ShaderFeature features, ShaderFeature bit)
{
    return (static_cast<std::uint32_t>(features) & static_cast<std::uint32_t>(bit)) != 0;
}

// Maps a permutation to a cooked HLSL entry. Cook-time #defines come later;
// the file names already encode the current set.
inline const char* pbr_vertex_shader(ShaderFeature features)
{
    if (has(features, ShaderFeature::Skinned)) return "pbr_skinned.vert";
    if (has(features, ShaderFeature::Instanced)) return "pbr_instanced.vert";
    return "pbr.vert";
}

inline const char* pbr_fragment_shader(ShaderFeature)
{
    return "pbr.frag";
}

} // namespace forge::render
