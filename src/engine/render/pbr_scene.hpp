#pragma once

#include "engine/anim/animator.hpp"
#include "engine/assets/scene.hpp"
#include "engine/render/ibl.hpp"
#include "engine/render/settings.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>
#include <vector>

namespace forge::render {

class PbrScene {
public:
    bool load(SDL_GPUDevice* device, const std::filesystem::path& path, std::string& error);
    bool ingest(SDL_GPUDevice* device, assets::Scene scene, std::string name, std::string& error);
    void destroy(SDL_GPUDevice* device);
    const assets::Scene& cpu() const { return cpu_; }
    void draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* culled,
              SDL_GPUGraphicsPipeline* double_sided, const DebugState& debug,
              const glm::vec3& camera_position, const ShadowInputs& shadows = {},
              const glm::vec4& color_mul = {1, 1, 1, 1}) const;
    void draw_skinned(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline,
                      const anim::Palette& palette, const DebugState& debug, const glm::vec3& camera_position) const;
    void draw_instanced(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline,
                        SDL_GPUBuffer* instances, Uint32 instance_count, const DebugState& debug,
                        const glm::vec3& camera_position) const;
    void draw_depth(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline) const;
    SDL_GPUTexture* dummy_shadow(SDL_GPUDevice* device);

    glm::vec3 bounds_min{0};
    glm::vec3 bounds_max{0};
    std::uint32_t triangle_count = 0;
    std::uint32_t material_count = 0;
    std::uint32_t texture_count = 0;
    std::string model_name;
    Uint32 specular_mips = 1;

private:
    struct GpuMaterial {
        assets::Material cpu;
        SDL_GPUSampler* samplers[assets::texture_slot_count]{};
        int images[assets::texture_slot_count]{-1, -1, -1, -1, -1};
    };

    SDL_GPUBuffer* vertices_ = nullptr;
    SDL_GPUBuffer* indices_ = nullptr;
    std::vector<rhi::Texture> images_;
    std::vector<GpuMaterial> materials_;
    std::vector<assets::Primitive> primitives_;
    std::vector<SDL_GPUSampler*> unique_samplers_;
    Ibl ibl_{};
    rhi::Texture white_{};
    SDL_GPUTexture* dummy_shadow_ = nullptr;
    rhi::Texture flat_normal_{};
    rhi::Texture default_orm_{};
    SDL_GPUSampler* ibl_sampler_ = nullptr;
    SDL_GPUSampler* lut_sampler_ = nullptr;
    Uint32 index_count_ = 0;
    assets::Scene cpu_{};
};

} // namespace forge::render
