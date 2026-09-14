#pragma once

#include "engine/rhi/command/command.hpp"
#include "engine/rhi/device/resources.hpp"

#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace forge::render {

enum class Access : std::uint8_t { Read, Write };

// Index + generation. Stale handles fail native() after clear/rebuild.
struct ResourceHandle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    explicit operator bool() const { return generation != 0; }
    bool operator==(const ResourceHandle&) const = default;
};

struct ResourceDesc {
    const char* name = "";
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    bool transient = true;
};

// Renderer builds this graph, then compile/execute. Transient GPU textures are
// allocated here via rhi::Resources — Renderer must not create extra transients
// after bind(). Barriers and aliasing of dead_after() resources are next.
class FrameGraph {
public:
    using Handle = ResourceHandle;
    static constexpr Handle kInvalid{};

    void bind(rhi::Resources& pool) { pool_ = &pool; }
    Handle import(const char* name, SDL_GPUTexture* texture);
    Handle create_transient(const ResourceDesc& desc);
    SDL_GPUTexture* native(Handle handle) const;
    Handle find(const char* name) const;

    void add_pass(const char* name, std::vector<Handle> reads, std::vector<Handle> writes,
                  std::function<bool(rhi::Command&)> execute);

    bool compile();
    bool execute(rhi::Command& command);
    void clear();

    std::size_t pass_count() const { return passes_.size(); }
    const char* pass_name(std::size_t index) const;
    int first_use(Handle handle) const;
    int last_use(Handle handle) const;
    bool transient(Handle handle) const;
    bool dead_after(Handle handle, int pass) const;

private:
    struct Resource {
        std::string name;
        SDL_GPUTexture* native = nullptr;
        std::uint32_t generation = 1;
        bool imported = false;
        bool transient = false;
        ResourceDesc desc{};
        rhi::TextureHandle gpu{};
        int first_pass = -1;
        int last_pass = -1;
    };
    struct Pass {
        std::string name;
        std::vector<Handle> reads;
        std::vector<Handle> writes;
        std::function<bool(rhi::Command&)> execute;
    };

    bool valid(Handle handle) const;
    void touch(Handle handle, int pass);

    rhi::Resources* pool_ = nullptr;
    std::vector<Resource> resources_;
    std::vector<Pass> passes_;
    bool compiled_ = false;
};

} // namespace forge::render
