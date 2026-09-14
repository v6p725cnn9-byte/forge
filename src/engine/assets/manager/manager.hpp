#pragma once

#include "engine/assets/gltf/scene.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace forge::assets {

enum class AssetState { Unloaded, Loading, CpuReady, GpuReady, Failed };

class Manager {
public:
    bool load_gltf(const std::filesystem::path& path, Scene& scene, std::string& error);
    AssetState state(const std::filesystem::path& path) const;
    void clear()
    {
        cache_.clear();
        state_.clear();
    }
    std::size_t cached() const { return cache_.size(); }

private:
    std::unordered_map<std::string, Scene> cache_;
    std::unordered_map<std::string, AssetState> state_;
};

Manager& catalog();

} // namespace forge::assets
