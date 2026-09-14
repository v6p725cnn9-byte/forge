#include "engine/assets/manager/manager.hpp"

namespace forge::assets {

bool Manager::load_gltf(const std::filesystem::path& path, Scene& scene, std::string& error)
{
    const auto key = path.lexically_normal().string();
    auto found = cache_.find(key);
    if (found == cache_.end()) {
        Scene loaded;
        if (!assets::load_gltf(path, loaded, error)) return false;
        found = cache_.emplace(key, std::move(loaded)).first;
    }
    scene = found->second;
    error.clear();
    return true;
}

Manager& catalog()
{
    static Manager manager;
    return manager;
}

} // namespace forge::assets
