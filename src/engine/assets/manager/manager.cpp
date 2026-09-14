#include "engine/assets/manager/manager.hpp"

namespace forge::assets {

bool Manager::load_gltf(const std::filesystem::path& path, Scene& scene, std::string& error)
{
    const auto key = path.lexically_normal().string();
    auto found = cache_.find(key);
    if (found == cache_.end()) {
        state_[key] = AssetState::Loading;
        Scene loaded;
        if (!assets::load_gltf(path, loaded, error)) {
            state_[key] = AssetState::Failed;
            return false;
        }
        found = cache_.emplace(key, std::move(loaded)).first;
        state_[key] = AssetState::CpuReady;
    }
    scene = found->second;
    error.clear();
    return true;
}

AssetState Manager::state(const std::filesystem::path& path) const
{
    const auto key = path.lexically_normal().string();
    const auto found = state_.find(key);
    if (found == state_.end()) return AssetState::Unloaded;
    return found->second;
}

Manager& catalog()
{
    static Manager manager;
    return manager;
}

} // namespace forge::assets
