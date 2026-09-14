#pragma once

#include "engine/assets/gltf/scene.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace forge::assets {

class Manager {
public:
    bool load_gltf(const std::filesystem::path& path, Scene& scene, std::string& error);
    void clear() { cache_.clear(); }
    std::size_t cached() const { return cache_.size(); }

private:
    std::unordered_map<std::string, Scene> cache_;
};

Manager& catalog();

} // namespace forge::assets
