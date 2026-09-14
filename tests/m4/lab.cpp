#include "lab.hpp"

#include "engine/render/pbr_pass.hpp"

#include <cstring>
#include <string>

namespace forge::labs {

bool StreamCityLab::setup(rhi::Host& host, Camera& camera)
{
    std::string error;
    if (!scene_.ingest(host.device(), assets::make_unit_cube(), "cube", error)) {
        SDL_Log("Cube ingest failed: %s", error.c_str());
        return false;
    }
    pipeline_ = render::make_pbr_instanced_pipeline(host, false);
    if (!pipeline_) return false;

    const auto count = world::populate_city(store_);
    index_.build(store_);
    instance_capacity_ = static_cast<Uint32>(store_.size());
    packed_.reserve(store_.size());

    SDL_GPUBufferCreateInfo buffer{};
    buffer.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer.size = instance_capacity_ * static_cast<Uint32>(sizeof(world::GpuInstance));
    instances_ = SDL_CreateGPUBuffer(host.device(), &buffer);
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer.size = buffer.size;
    transfer_ = SDL_CreateGPUTransferBuffer(host.device(), &transfer);
    if (!instances_ || !transfer_) return false;

    debug_.lab = "m4-stream";
    debug_.model = scene_.model_name.c_str();
    debug_.materials = scene_.material_count;
    debug_.textures = scene_.texture_count;
    debug_.stream_radius = 96.0f;
    debug_.frustum_cull = true;
    debug_.help = "M4 streaming | RMB+WASD fly | Shift boost | F1 overlay";
    camera.position = {12.0f, 14.0f, 12.0f};
    camera.yaw = 35.0f;
    camera.pitch = -18.0f;
    camera.speed = 28.0f;
    camera.near_plane = 0.2f;
    camera.far_plane = 280.0f;
    debug_.home_position = camera.position;
    debug_.home_yaw = camera.yaw;
    debug_.home_pitch = camera.pitch;
    SDL_Log("M4 city: %u entities in %zu sectors", count, index_.sector_count());
    return true;
}

void StreamCityLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.captured) camera.move(input.move, dt, input.boost);
    index_.stream(store_, camera.position, debug_.stream_radius, stats_);
    debug_.sectors_loaded = stats_.sectors_loaded;
    debug_.entities_streamed = stats_.entities_streamed;
}

bool StreamCityLab::upload_instances(rhi::Host& host, rhi::Command& command)
{
    const auto bytes = static_cast<Uint32>(packed_.size() * sizeof(world::GpuInstance));
    auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(host.device(), transfer_, true));
    if (!mapped) return false;
    std::memcpy(mapped, packed_.data(), bytes);
    SDL_UnmapGPUTransferBuffer(host.device(), transfer_);
    auto* copy = SDL_BeginGPUCopyPass(command.handle);
    if (!copy) return false;
    const SDL_GPUTransferBufferLocation source{transfer_, 0};
    const SDL_GPUBufferRegion dest{instances_, 0, bytes};
    SDL_UploadToGPUBuffer(copy, &source, &dest, true);
    SDL_EndGPUCopyPass(copy);
    return true;
}

rhi::FrameResult StreamCityLab::draw(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain,
                                     Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!host.resize(width, height, host_config())) return rhi::FrameResult::failed;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 view_projection = camera.projection(aspect) * camera.view();
    const auto frustum = world::frustum_from_clip(view_projection);
    world::collect_instances(store_, debug_.frustum_cull ? &frustum : nullptr, 0, packed_, stats_);
    debug_.instances_drawn = stats_.instances_drawn;
    debug_.instances_culled = stats_.instances_culled;
    debug_.sectors_loaded = stats_.sectors_loaded;
    debug_.entities_streamed = stats_.entities_streamed;

    if (!packed_.empty() && !upload_instances(host, command)) return rhi::FrameResult::failed;

    render::CameraViewUniforms camera_ubo{};
    camera_ubo.view_projection = view_projection;
    SDL_PushGPUVertexUniformData(command.handle, 0, &camera_ubo, sizeof(camera_ubo));

    SDL_GPUColorTargetInfo color{};
    color.texture = host.hdr();
    color.clear_color = {0.03f, 0.045f, 0.07f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = host.depth();
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.cycle = true;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, &depth);
    if (!pass) return rhi::FrameResult::failed;
    scene_.draw_instanced(command.handle, pass, pipeline_, instances_, static_cast<Uint32>(packed_.size()),
                          debug_, camera.position);
    SDL_EndGPURenderPass(pass);
    if (!render::apply_tonemap(host, command, swapchain, debug_.exposure, 0.0f)) return rhi::FrameResult::failed;
    return rhi::FrameResult::presented;
}

void StreamCityLab::teardown(rhi::Host& host)
{
    scene_.destroy(host.device());
    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pipeline_);
    if (instances_) SDL_ReleaseGPUBuffer(host.device(), instances_);
    if (transfer_) SDL_ReleaseGPUTransferBuffer(host.device(), transfer_);
    pipeline_ = nullptr;
    instances_ = nullptr;
    transfer_ = nullptr;
}

} // namespace forge::labs
