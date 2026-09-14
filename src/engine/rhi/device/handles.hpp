#pragma once

#include <cstdint>

namespace forge::rhi {

template <typename Tag>
struct Handle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    explicit operator bool() const { return generation != 0; }
    bool operator==(const Handle&) const = default;
};

struct TextureTag;
struct BufferTag;
struct SamplerTag;
struct PipelineTag;
struct ShaderTag;

using TextureHandle = Handle<TextureTag>;
using BufferHandle = Handle<BufferTag>;
using SamplerHandle = Handle<SamplerTag>;
using PipelineHandle = Handle<PipelineTag>;
using ShaderHandle = Handle<ShaderTag>;

} // namespace forge::rhi
