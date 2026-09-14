#include "engine/render/passes/opaque/pbr_scene.hpp"

#include "engine/rhi/command/command.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace forge::render {
namespace {

struct CameraUniforms {
    glm::mat4 view_projection;
    glm::mat4 model;
};
static_assert(sizeof(CameraUniforms) == 128);

struct FrameShading {
    glm::vec4 camera_pos_mips;
    glm::vec4 light_dir_ibl;
    glm::vec4 light_color_exposure;
    glm::vec4 base_color;
    glm::vec4 emissive_metallic;
    glm::vec4 params;
    glm::vec4 flags;
    glm::vec4 tex_u[5];
    glm::vec4 tex_v[5];
    glm::vec4 cascade_splits;
    glm::vec4 shadow_params;
    glm::vec4 fog_color_density;
    glm::mat4 light_vp[3];
};
static_assert(sizeof(FrameShading) == 512);

struct SamplerKey {
    int min_filter;
    int mag_filter;
    int wrap_s;
    int wrap_t;
    bool operator==(const SamplerKey& other) const = default;
};
struct SamplerHash {
    std::size_t operator()(const SamplerKey& key) const
    {
        return (static_cast<std::size_t>(key.min_filter) * 131u)
            ^ (static_cast<std::size_t>(key.mag_filter) * 17u)
            ^ (static_cast<std::size_t>(key.wrap_s) << 8)
            ^ static_cast<std::size_t>(key.wrap_t);
    }
};

bool upload_geometry(SDL_GPUDevice* device, const assets::Scene& scene, SDL_GPUBuffer*& vertices,
                     SDL_GPUBuffer*& indices)
{
    const auto vertex_bytes = static_cast<Uint32>(scene.vertices.size() * sizeof(assets::Vertex));
    const auto index_bytes = static_cast<Uint32>(scene.indices.size() * sizeof(Uint32));
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = vertex_bytes;
    vertices = SDL_CreateGPUBuffer(device, &info);
    info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    info.size = index_bytes;
    indices = SDL_CreateGPUBuffer(device, &info);
    if (!vertices || !indices) return false;

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = vertex_bytes + index_bytes;
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) return false;
    auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device, transfer, false));
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    std::memcpy(mapped, scene.vertices.data(), vertex_bytes);
    std::memcpy(mapped + vertex_bytes, scene.indices.data(), index_bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    rhi::Command command(device);
    if (!command.handle) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    auto* pass = SDL_BeginGPUCopyPass(command.handle);
    if (!pass) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    const SDL_GPUTransferBufferLocation vertex_source{transfer, 0};
    const SDL_GPUTransferBufferLocation index_source{transfer, vertex_bytes};
    const SDL_GPUBufferRegion vertex_dest{vertices, 0, vertex_bytes};
    const SDL_GPUBufferRegion index_dest{indices, 0, index_bytes};
    SDL_UploadToGPUBuffer(pass, &vertex_source, &vertex_dest, false);
    SDL_UploadToGPUBuffer(pass, &index_source, &index_dest, false);
    SDL_EndGPUCopyPass(pass);
    const bool ok = command.submit();
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return ok;
}

} // namespace

bool PbrScene::load(SDL_GPUDevice* device, const std::filesystem::path& path, std::string& error)
{
    assets::Scene scene;
    if (!assets::load_gltf(path, scene, error)) return false;
    return ingest(device, std::move(scene), path.stem().string(), error);
}

bool PbrScene::ingest(SDL_GPUDevice* device, assets::Scene scene, std::string name, std::string& error,
                      std::shared_ptr<Ibl> lighting)
{
    destroy(device);
    for (const auto& warning : scene.warnings) SDL_Log("glTF: %s", warning.c_str());

    if (!upload_geometry(device, scene, vertices_, indices_)) {
        error = std::string("Geometry upload failed: ") + SDL_GetError();
        destroy(device);
        return false;
    }
    index_count_ = static_cast<Uint32>(scene.indices.size());
    primitives_ = scene.primitives;
    bounds_min = scene.bounds_min;
    bounds_max = scene.bounds_max;
    triangle_count = index_count_ / 3;
    model_name = std::move(name);

    white_ = rhi::solid_texture(device, {255, 255, 255, 255}, false);
    flat_normal_ = rhi::solid_texture(device, {128, 128, 255, 255}, false);
    default_orm_ = rhi::solid_texture(device, {255, 255, 255, 255}, false);
    if (!white_.handle || !flat_normal_.handle || !default_orm_.handle) {
        error = "Cannot create default PBR textures";
        destroy(device);
        return false;
    }
    ibl_ = std::move(lighting);
    if (!ibl_) {
        ibl_ = std::shared_ptr<Ibl>(new Ibl{}, [device](Ibl* lighting) {
            destroy_ibl(device, *lighting);
            delete lighting;
        });
        if (!create_studio_ibl(device, *ibl_, error)) {
            destroy(device);
            return false;
        }
    }
    specular_mips = ibl_->specular_mips;

    assets::TextureRef clamp;
    clamp.min_filter = 9987;
    clamp.mag_filter = 9729;
    clamp.wrap_s = 33071;
    clamp.wrap_t = 33071;
    ibl_sampler_ = rhi::create_sampler(device, clamp, true);
    lut_sampler_ = rhi::create_sampler(device, clamp, true);
    if (!ibl_sampler_ || !lut_sampler_) {
        error = "Cannot create IBL samplers";
        destroy(device);
        return false;
    }

    std::vector<char> srgb(scene.images.size(), 0);
    for (const auto& material : scene.materials) {
        if (material.textures[assets::base_color].image >= 0)
            srgb[static_cast<std::size_t>(material.textures[assets::base_color].image)] = 1;
        if (material.textures[assets::emissive].image >= 0)
            srgb[static_cast<std::size_t>(material.textures[assets::emissive].image)] = 1;
    }

    images_.resize(scene.images.size());
    for (std::size_t i = 0; i < scene.images.size(); ++i) {
        if (scene.images[i].path.empty() && scene.images[i].bytes.empty()) continue;
        images_[i] = rhi::load_texture(device, scene.images[i], srgb[i] != 0, error);
        if (!images_[i].handle) {
            destroy(device);
            return false;
        }
        ++texture_count;
    }

    std::unordered_map<SamplerKey, SDL_GPUSampler*, SamplerHash> cache;
    materials_.resize(scene.materials.size());
    for (std::size_t i = 0; i < scene.materials.size(); ++i) {
        materials_[i].cpu = scene.materials[i];
        for (int slot = 0; slot < assets::texture_slot_count; ++slot) {
            const auto& ref = scene.materials[i].textures[slot];
            materials_[i].images[slot] = ref.image;
            const SamplerKey key{ref.min_filter, ref.mag_filter, ref.wrap_s, ref.wrap_t};
            auto& sampler = cache[key];
            if (!sampler) {
                sampler = rhi::create_sampler(device, ref);
                if (!sampler) {
                    error = "Cannot create material sampler";
                    destroy(device);
                    return false;
                }
                unique_samplers_.push_back(sampler);
            }
            materials_[i].samplers[slot] = sampler;
        }
    }
    material_count = static_cast<std::uint32_t>(scene.materials.size());
    cpu_.nodes = std::move(scene.nodes);
    cpu_.skins = std::move(scene.skins);
    cpu_.animations = std::move(scene.animations);
    if (!dummy_shadow(device)) {
        error = "Cannot create dummy shadow map";
        destroy(device);
        return false;
    }
    error.clear();
    SDL_Log("Loaded %s: %u triangles, %u materials, %u textures", model_name.c_str(), triangle_count,
            material_count, texture_count);
    return true;
}

void PbrScene::destroy(SDL_GPUDevice* device)
{
    if (!device) return;
    ibl_.reset();
    for (auto& texture : images_) rhi::destroy_texture(device, texture);
    images_.clear();
    rhi::destroy_texture(device, white_);
    rhi::destroy_texture(device, flat_normal_);
    rhi::destroy_texture(device, default_orm_);
    for (auto* sampler : unique_samplers_) SDL_ReleaseGPUSampler(device, sampler);
    unique_samplers_.clear();
    if (ibl_sampler_) SDL_ReleaseGPUSampler(device, ibl_sampler_);
    if (lut_sampler_) SDL_ReleaseGPUSampler(device, lut_sampler_);
    ibl_sampler_ = lut_sampler_ = nullptr;
    if (dummy_shadow_) SDL_ReleaseGPUTexture(device, dummy_shadow_);
    dummy_shadow_ = nullptr;
    if (vertices_) SDL_ReleaseGPUBuffer(device, vertices_);
    if (indices_) SDL_ReleaseGPUBuffer(device, indices_);
    vertices_ = indices_ = nullptr;
    materials_.clear();
    primitives_.clear();
    cpu_ = {};
    triangle_count = material_count = texture_count = 0;
    index_count_ = 0;
}

SDL_GPUTexture* PbrScene::dummy_shadow(SDL_GPUDevice* device)
{
    if (dummy_shadow_) return dummy_shadow_;
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = 4;
    info.height = 4;
    info.layer_count_or_depth = 3;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    dummy_shadow_ = SDL_CreateGPUTexture(device, &info);
    return dummy_shadow_;
}

void PbrScene::draw_depth(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline) const
{
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUBufferBinding vertex_binding{vertices_, 0};
    const SDL_GPUBufferBinding index_binding{indices_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, index_count_, 1, 0, 0, 0);
}

void PbrScene::draw(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* culled,
                    SDL_GPUGraphicsPipeline* double_sided, const DebugState& debug,
                    const glm::vec3& camera_position, const ShadowInputs& shadows, const glm::vec4& color_mul) const
{
    const SDL_GPUBufferBinding vertex_binding{vertices_, 0};
    const SDL_GPUBufferBinding index_binding{indices_, 0};
    bool bound_double = true;
    const auto bind_pipeline = [&](bool double_sided_material) {
        if (double_sided_material == bound_double) return;
        SDL_BindGPUGraphicsPipeline(pass, double_sided_material ? double_sided : culled);
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
        SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        bound_double = double_sided_material;
    };
    bind_pipeline(false);

    SDL_GPUTexture* shadow_map = shadows.map ? shadows.map : dummy_shadow_;
    SDL_GPUSampler* shadow_sampler = shadows.sampler ? shadows.sampler : lut_sampler_;

    for (const auto& primitive : primitives_) {
        const auto& material = materials_[primitive.material];
        bind_pipeline(material.cpu.double_sided);

        const auto texture_for = [&](int slot, const rhi::Texture& fallback) -> SDL_GPUTexture* {
            const int image = material.images[slot];
            if (image >= 0 && images_[static_cast<std::size_t>(image)].handle)
                return images_[static_cast<std::size_t>(image)].handle;
            return fallback.handle;
        };

        const SDL_GPUTextureSamplerBinding bindings[9] = {
            {texture_for(assets::base_color, white_), material.samplers[assets::base_color]},
            {texture_for(assets::metallic_roughness, default_orm_), material.samplers[assets::metallic_roughness]},
            {texture_for(assets::normal, flat_normal_), material.samplers[assets::normal]},
            {texture_for(assets::occlusion, white_), material.samplers[assets::occlusion]},
            {texture_for(assets::emissive, white_), material.samplers[assets::emissive]},
            {ibl_->irradiance.handle, ibl_sampler_},
            {ibl_->specular.handle, ibl_sampler_},
            {ibl_->brdf.handle, lut_sampler_},
            {shadow_map, shadow_sampler},
        };
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 9);

        FrameShading shading{};
        shading.camera_pos_mips = glm::vec4{camera_position, static_cast<float>(specular_mips)};
        shading.light_dir_ibl = glm::vec4{sun_direction(debug.light_azimuth, debug.light_elevation), debug.ibl_intensity};
        shading.light_color_exposure = glm::vec4{debug.light_color * debug.light_intensity, debug.exposure};
        shading.fog_color_density = glm::vec4{debug.fog_color, debug.fog_density};
        shading.base_color = material.cpu.base_color_factor * color_mul;
        shading.emissive_metallic = glm::vec4{material.cpu.emissive_factor, material.cpu.metallic};
        shading.params = {material.cpu.roughness, material.cpu.normal_scale, material.cpu.occlusion_strength,
                          material.cpu.alpha_cutoff};
        shading.flags = {material.cpu.unlit ? 1.0f : 0.0f, material.cpu.alpha_mask ? 1.0f : 0.0f,
                         shadows.enabled ? 1.0f : 0.0f, 0.0f};
        for (int slot = 0; slot < assets::texture_slot_count; ++slot) {
            shading.tex_u[slot] = material.cpu.textures[slot].transform.u;
            shading.tex_v[slot] = material.cpu.textures[slot].transform.v;
        }
        shading.cascade_splits = shadows.splits;
        shading.shadow_params = {shadows.texel, shadows.bias, shadows.strength, shadows.enabled ? 1.0f : 0.0f};
        shading.light_vp[0] = shadows.light_vp[0];
        shading.light_vp[1] = shadows.light_vp[1];
        shading.light_vp[2] = shadows.light_vp[2];
        SDL_PushGPUFragmentUniformData(command, 0, &shading, sizeof(shading));
        SDL_DrawGPUIndexedPrimitives(pass, primitive.index_count, 1, primitive.first_index, 0, 0);
    }
}

void PbrScene::draw_instanced(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass,
                              SDL_GPUGraphicsPipeline* pipeline, SDL_GPUBuffer* instances, Uint32 instance_count,
                              const DebugState& debug, const glm::vec3& camera_position) const
{
    if (!pipeline || !instances || instance_count == 0 || primitives_.empty()) return;
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUBufferBinding vertex_bindings[2] = {{vertices_, 0}, {instances, 0}};
    const SDL_GPUBufferBinding index_binding{indices_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings, 2);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUTexture* shadow_map = dummy_shadow_;
    SDL_GPUSampler* shadow_sampler = lut_sampler_;

    for (const auto& primitive : primitives_) {
        const auto& material = materials_[primitive.material];
        const auto texture_for = [&](int slot, const rhi::Texture& fallback) -> SDL_GPUTexture* {
            const int image = material.images[slot];
            if (image >= 0 && images_[static_cast<std::size_t>(image)].handle)
                return images_[static_cast<std::size_t>(image)].handle;
            return fallback.handle;
        };
        const SDL_GPUTextureSamplerBinding bindings[9] = {
            {texture_for(assets::base_color, white_), material.samplers[assets::base_color]},
            {texture_for(assets::metallic_roughness, default_orm_), material.samplers[assets::metallic_roughness]},
            {texture_for(assets::normal, flat_normal_), material.samplers[assets::normal]},
            {texture_for(assets::occlusion, white_), material.samplers[assets::occlusion]},
            {texture_for(assets::emissive, white_), material.samplers[assets::emissive]},
            {ibl_->irradiance.handle, ibl_sampler_},
            {ibl_->specular.handle, ibl_sampler_},
            {ibl_->brdf.handle, lut_sampler_},
            {shadow_map, shadow_sampler},
        };
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 9);

        FrameShading shading{};
        shading.camera_pos_mips = glm::vec4{camera_position, static_cast<float>(specular_mips)};
        shading.light_dir_ibl = glm::vec4{sun_direction(debug.light_azimuth, debug.light_elevation), debug.ibl_intensity};
        shading.light_color_exposure = glm::vec4{debug.light_color * debug.light_intensity, debug.exposure};
        shading.fog_color_density = glm::vec4{debug.fog_color, debug.fog_density};
        shading.base_color = material.cpu.base_color_factor;
        shading.emissive_metallic = glm::vec4{material.cpu.emissive_factor, material.cpu.metallic};
        shading.params = {material.cpu.roughness, material.cpu.normal_scale, material.cpu.occlusion_strength,
                          material.cpu.alpha_cutoff};
        shading.flags = {material.cpu.unlit ? 1.0f : 0.0f, material.cpu.alpha_mask ? 1.0f : 0.0f, 0.0f, 0.0f};
        for (int slot = 0; slot < assets::texture_slot_count; ++slot) {
            shading.tex_u[slot] = material.cpu.textures[slot].transform.u;
            shading.tex_v[slot] = material.cpu.textures[slot].transform.v;
        }
        SDL_PushGPUFragmentUniformData(command, 0, &shading, sizeof(shading));
        SDL_DrawGPUIndexedPrimitives(pass, primitive.index_count, instance_count, primitive.first_index, 0, 0);
    }
}

void PbrScene::draw_skinned(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline,
                            const anim::Palette& palette, const DebugState& debug,
                            const glm::vec3& camera_position) const
{
    if (!pipeline || primitives_.empty()) return;
    SDL_PushGPUVertexUniformData(command, 1, palette.joints, sizeof(palette.joints));
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUBufferBinding vertex_binding{vertices_, 0};
    const SDL_GPUBufferBinding index_binding{indices_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUTexture* shadow_map = dummy_shadow_;
    SDL_GPUSampler* shadow_sampler = lut_sampler_;

    for (const auto& primitive : primitives_) {
        const auto& material = materials_[primitive.material];
        const auto texture_for = [&](int slot, const rhi::Texture& fallback) -> SDL_GPUTexture* {
            const int image = material.images[slot];
            if (image >= 0 && images_[static_cast<std::size_t>(image)].handle)
                return images_[static_cast<std::size_t>(image)].handle;
            return fallback.handle;
        };
        const SDL_GPUTextureSamplerBinding bindings[9] = {
            {texture_for(assets::base_color, white_), material.samplers[assets::base_color]},
            {texture_for(assets::metallic_roughness, default_orm_), material.samplers[assets::metallic_roughness]},
            {texture_for(assets::normal, flat_normal_), material.samplers[assets::normal]},
            {texture_for(assets::occlusion, white_), material.samplers[assets::occlusion]},
            {texture_for(assets::emissive, white_), material.samplers[assets::emissive]},
            {ibl_->irradiance.handle, ibl_sampler_},
            {ibl_->specular.handle, ibl_sampler_},
            {ibl_->brdf.handle, lut_sampler_},
            {shadow_map, shadow_sampler},
        };
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 9);

        FrameShading shading{};
        shading.camera_pos_mips = glm::vec4{camera_position, static_cast<float>(specular_mips)};
        shading.light_dir_ibl = glm::vec4{sun_direction(debug.light_azimuth, debug.light_elevation), debug.ibl_intensity};
        shading.light_color_exposure = glm::vec4{debug.light_color * debug.light_intensity, debug.exposure};
        shading.fog_color_density = glm::vec4{debug.fog_color, debug.fog_density};
        shading.base_color = material.cpu.base_color_factor;
        shading.emissive_metallic = glm::vec4{material.cpu.emissive_factor, material.cpu.metallic};
        shading.params = {material.cpu.roughness, material.cpu.normal_scale, material.cpu.occlusion_strength,
                          material.cpu.alpha_cutoff};
        shading.flags = {material.cpu.unlit ? 1.0f : 0.0f, material.cpu.alpha_mask ? 1.0f : 0.0f, 0.0f, 0.0f};
        for (int slot = 0; slot < assets::texture_slot_count; ++slot) {
            shading.tex_u[slot] = material.cpu.textures[slot].transform.u;
            shading.tex_v[slot] = material.cpu.textures[slot].transform.v;
        }
        SDL_PushGPUFragmentUniformData(command, 0, &shading, sizeof(shading));
        SDL_DrawGPUIndexedPrimitives(pass, primitive.index_count, 1, primitive.first_index, 0, 0);
    }
}

} // namespace forge::render
