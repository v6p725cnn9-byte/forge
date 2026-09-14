#pragma once

#include "engine/rhi/command/command.hpp"

#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace forge::render {

// CPU-side pass graph. Records named textures and a linear pass list.
// Lifetime/transitions stay explicit for now; compile() checks that every
// read was written or imported. Execute runs in recorded order.
class FrameGraph {
public:
    using Handle = std::uint32_t;
    static constexpr Handle kInvalid = 0;

    Handle import(const char* name, SDL_GPUTexture* texture);
    SDL_GPUTexture* native(Handle handle) const;
    Handle find(const char* name) const;

    void add_pass(const char* name, std::vector<Handle> reads, std::vector<Handle> writes,
                  std::function<bool(rhi::Command&)> execute);

    bool compile();
    bool execute(rhi::Command& command);
    void clear();

    std::size_t pass_count() const { return passes_.size(); }
    const char* pass_name(std::size_t index) const;

private:
    struct Resource {
        std::string name;
        SDL_GPUTexture* native = nullptr;
        bool imported = false;
        bool written = false;
    };
    struct Pass {
        std::string name;
        std::vector<Handle> reads;
        std::vector<Handle> writes;
        std::function<bool(rhi::Command&)> execute;
    };

    std::vector<Resource> resources_;
    std::vector<Pass> passes_;
    bool compiled_ = false;
};

} // namespace forge::render
