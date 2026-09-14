#include "engine/render/framegraph/framegraph.hpp"

#include <cstring>

namespace forge::render {

FrameGraph::Handle FrameGraph::import(const char* name, SDL_GPUTexture* texture)
{
    compiled_ = false;
    if (!name || !texture) return kInvalid;
    resources_.push_back({name, texture, true, true});
    return static_cast<Handle>(resources_.size());
}

SDL_GPUTexture* FrameGraph::native(Handle handle) const
{
    if (handle == kInvalid || handle > resources_.size()) return nullptr;
    return resources_[handle - 1].native;
}

FrameGraph::Handle FrameGraph::find(const char* name) const
{
    if (!name) return kInvalid;
    for (std::size_t i = 0; i < resources_.size(); ++i) {
        if (resources_[i].name == name) return static_cast<Handle>(i + 1);
    }
    return kInvalid;
}

void FrameGraph::add_pass(const char* name, std::vector<Handle> reads, std::vector<Handle> writes,
                          std::function<bool(rhi::Command&)> execute)
{
    compiled_ = false;
    passes_.push_back({name ? name : "", std::move(reads), std::move(writes), std::move(execute)});
}

bool FrameGraph::compile()
{
    for (auto& resource : resources_)
        if (!resource.imported) resource.written = false;
    for (const auto& pass : passes_) {
        for (Handle handle : pass.reads) {
            if (handle == kInvalid || handle > resources_.size()) return false;
            if (!resources_[handle - 1].written) return false;
        }
        for (Handle handle : pass.writes) {
            if (handle == kInvalid || handle > resources_.size()) return false;
            resources_[handle - 1].written = true;
        }
        if (!pass.execute) return false;
    }
    compiled_ = true;
    return true;
}

bool FrameGraph::execute(rhi::Command& command)
{
    if (!compiled_ && !compile()) return false;
    for (auto& pass : passes_) {
        if (!pass.execute(command)) return false;
    }
    return true;
}

void FrameGraph::clear()
{
    resources_.clear();
    passes_.clear();
    compiled_ = false;
}

const char* FrameGraph::pass_name(std::size_t index) const
{
    if (index >= passes_.size()) return "";
    return passes_[index].name.c_str();
}

} // namespace forge::render
