#include "engine/render/framegraph/framegraph.hpp"

namespace forge::render {

bool FrameGraph::valid(Handle handle) const
{
    if (handle.index == 0 || handle.index > resources_.size()) return false;
    return resources_[handle.index - 1].generation == handle.generation;
}

void FrameGraph::touch(Handle handle, int pass)
{
    if (!valid(handle)) return;
    auto& resource = resources_[handle.index - 1];
    if (resource.first_pass < 0 || pass < resource.first_pass) resource.first_pass = pass;
    if (pass > resource.last_pass) resource.last_pass = pass;
}

FrameGraph::Handle FrameGraph::import(const char* name, SDL_GPUTexture* texture)
{
    compiled_ = false;
    if (!name || !texture) return kInvalid;
    resources_.push_back({name, texture, 1, true, false, {}, {}, -1, -1});
    return {static_cast<std::uint32_t>(resources_.size()), 1};
}

FrameGraph::Handle FrameGraph::create_transient(const ResourceDesc& desc)
{
    compiled_ = false;
    if (!desc.name) return kInvalid;
    rhi::TextureHandle gpu{};
    SDL_GPUTexture* native = nullptr;
    if (pool_ && desc.width > 0 && desc.height > 0 && desc.format != SDL_GPU_TEXTUREFORMAT_INVALID) {
        gpu = pool_->create_texture({desc.format, desc.usage, desc.width, desc.height});
        native = pool_->native(gpu);
        if (!native) return kInvalid;
    }
    resources_.push_back({desc.name, native, 1, false, desc.transient, desc, gpu, -1, -1});
    return {static_cast<std::uint32_t>(resources_.size()), 1};
}

SDL_GPUTexture* FrameGraph::native(Handle handle) const
{
    if (!valid(handle)) return nullptr;
    return resources_[handle.index - 1].native;
}

FrameGraph::Handle FrameGraph::find(const char* name) const
{
    if (!name) return kInvalid;
    for (std::size_t i = 0; i < resources_.size(); ++i) {
        if (resources_[i].name == name) return {static_cast<std::uint32_t>(i + 1), resources_[i].generation};
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
    for (auto& resource : resources_) {
        resource.first_pass = -1;
        resource.last_pass = -1;
    }
    std::vector<char> written(resources_.size(), 0);
    for (auto& resource : resources_)
        if (resource.imported) written[&resource - resources_.data()] = 1;

    for (int i = 0; i < static_cast<int>(passes_.size()); ++i) {
        const auto& pass = passes_[static_cast<std::size_t>(i)];
        if (!pass.execute) return false;
        for (Handle handle : pass.reads) {
            if (!valid(handle)) return false;
            if (!written[handle.index - 1]) return false;
            touch(handle, i);
        }
        for (Handle handle : pass.writes) {
            if (!valid(handle)) return false;
            written[handle.index - 1] = 1;
            touch(handle, i);
        }
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
    if (pool_) {
        for (auto& resource : resources_) {
            if (resource.transient && resource.gpu) pool_->destroy(resource.gpu);
            resource.gpu = {};
            resource.native = nullptr;
            ++resource.generation;
        }
    } else {
        for (auto& resource : resources_) ++resource.generation;
    }
    resources_.clear();
    passes_.clear();
    compiled_ = false;
}

const char* FrameGraph::pass_name(std::size_t index) const
{
    if (index >= passes_.size()) return "";
    return passes_[index].name.c_str();
}

int FrameGraph::first_use(Handle handle) const
{
    if (!valid(handle)) return -1;
    return resources_[handle.index - 1].first_pass;
}

int FrameGraph::last_use(Handle handle) const
{
    if (!valid(handle)) return -1;
    return resources_[handle.index - 1].last_pass;
}

bool FrameGraph::transient(Handle handle) const
{
    return valid(handle) && resources_[handle.index - 1].transient;
}

bool FrameGraph::dead_after(Handle handle, int pass) const
{
    const int last = last_use(handle);
    return last >= 0 && pass > last;
}

} // namespace forge::render
