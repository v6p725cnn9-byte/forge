#include "lab.hpp"

#include "engine/rhi/shader/shader.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

namespace forge::labs {
namespace {

struct Vertex {
    std::array<float, 3> position;
    std::array<float, 3> color;
};

void quad(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices,
          const std::array<glm::vec3, 4>& corners, glm::vec3 color)
{
    const auto base = static_cast<std::uint32_t>(vertices.size());
    for (const auto& p : corners) vertices.push_back({{p.x, p.y, p.z}, {color.r, color.g, color.b}});
    for (const auto index : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(base + index);
}

void box(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, glm::vec3 center, glm::vec3 size,
         glm::vec3 color)
{
    const auto a = center - size * 0.5f;
    const auto b = center + size * 0.5f;
    quad(vertices, indices, {{{a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}}}, color * 0.82f);
    quad(vertices, indices, {{{b.x, a.y, a.z}, {a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}}}, color * 0.60f);
    quad(vertices, indices, {{{a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}}}, color * 0.65f);
    quad(vertices, indices, {{{b.x, a.y, b.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}}}, color * 0.90f);
    quad(vertices, indices, {{{a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z}}}, color);
    quad(vertices, indices, {{{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}}}, color * 0.45f);
}

} // namespace

bool UnlitCubesLab::setup(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer, Camera& camera)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    box(vertices, indices, {0.0f, 1.0f, 0.0f}, {2.0f, 2.0f, 2.0f}, {0.95f, 0.40f, 0.16f});
    box(vertices, indices, {-4.0f, 2.0f, -3.0f}, {2.0f, 4.0f, 2.0f}, {0.17f, 0.64f, 0.75f});
    box(vertices, indices, {3.0f, 0.5f, -2.0f}, {3.0f, 1.0f, 2.0f}, {0.90f, 0.71f, 0.26f});
    box(vertices, indices, {-1.5f, 0.75f, 4.0f}, {1.5f, 1.5f, 1.5f}, {0.38f, 0.70f, 0.44f});
    box(vertices, indices, {3.0f, 2.5f, -7.0f}, {1.5f, 5.0f, 1.5f}, {0.58f, 0.46f, 0.82f});
    for (int z = -12; z < 12; ++z) {
        for (int x = -12; x < 12; ++x) {
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);
            const auto color = ((x + z) % 2 == 0) ? glm::vec3{0.19f, 0.23f, 0.28f} : glm::vec3{0.23f, 0.28f, 0.33f};
            quad(vertices, indices, {{{fx, 0, fz + 1}, {fx + 1, 0, fz + 1}, {fx + 1, 0, fz}, {fx, 0, fz}}}, color);
        }
    }
    index_count_ = static_cast<Uint32>(indices.size());

    const auto vertex_bytes = static_cast<Uint32>(vertices.size() * sizeof(Vertex));
    const auto index_bytes = static_cast<Uint32>(indices.size() * sizeof(Uint32));
    SDL_GPUBufferCreateInfo buffer{};
    buffer.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer.size = vertex_bytes;
    vertices_ = SDL_CreateGPUBuffer(host.device(), &buffer);
    buffer.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    buffer.size = index_bytes;
    indices_ = SDL_CreateGPUBuffer(host.device(), &buffer);
    if (!vertices_ || !indices_) return false;

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = vertex_bytes + index_bytes;
    auto* transfer = SDL_CreateGPUTransferBuffer(host.device(), &transfer_info);
    auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(host.device(), transfer, false));
    if (!mapped || !transfer) return false;
    std::memcpy(mapped, vertices.data(), vertex_bytes);
    std::memcpy(mapped + vertex_bytes, indices.data(), index_bytes);
    SDL_UnmapGPUTransferBuffer(host.device(), transfer);
    rhi::Command command(host.device());
    auto* copy = SDL_BeginGPUCopyPass(command.handle);
    const SDL_GPUTransferBufferLocation vsrc{transfer, 0};
    const SDL_GPUTransferBufferLocation isrc{transfer, vertex_bytes};
    const SDL_GPUBufferRegion vdst{vertices_, 0, vertex_bytes};
    const SDL_GPUBufferRegion idst{indices_, 0, index_bytes};
    SDL_UploadToGPUBuffer(copy, &vsrc, &vdst, false);
    SDL_UploadToGPUBuffer(copy, &isrc, &idst, false);
    SDL_EndGPUCopyPass(copy);
    const bool ok = command.submit();
    SDL_ReleaseGPUTransferBuffer(host.device(), transfer);
    if (!ok) return false;

    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"unlit.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"unlit.frag", SDL_GPU_SHADERSTAGE_FRAGMENT});
    if (!vs || !fs) return false;
    SDL_GPUVertexBufferDescription vb{};
    vb.pitch = sizeof(Vertex);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(Vertex, position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(Vertex, color))},
    };
    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GetGPUSwapchainTextureFormat(host.device(), host.window());
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vb;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 2;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = renderer.depth_format();
    pipeline_ = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    camera = Camera{};
    debug_.lab = "m1-unlit";
    debug_.model = "cubes";
    debug_.help = "M1 lab | RMB+WASD fly | F1 overlay";
    SDL_Log("M1 unlit cubes: %u triangles", index_count_ / 3);
    return pipeline_ != nullptr;
}

void UnlitCubesLab::update(float dt, Camera& camera, const app::LabInput& input)
{
    if (input.captured) camera.move(input.move, dt, input.boost);
}

rhi::FrameResult UnlitCubesLab::draw(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer, rhi::Command& command, SDL_GPUTexture* swapchain,
                                     Uint32 width, Uint32 height, Camera& camera, bool)
{
    if (!renderer.ensure(host, width, height, frame_config())) return rhi::FrameResult::failed;
    const glm::mat4 vp = camera.projection(static_cast<float>(width) / static_cast<float>(height)) * camera.view();
    SDL_PushGPUVertexUniformData(command.handle, 0, glm::value_ptr(vp), sizeof(vp));

    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = {0.045f, 0.060f, 0.085f, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = renderer.depth();
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.cycle = true;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, &depth);
    if (!pass) return rhi::FrameResult::failed;
    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    const SDL_GPUBufferBinding vb{vertices_, 0};
    const SDL_GPUBufferBinding ib{indices_, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, index_count_, 1, 0, 0, 0);
    SDL_EndGPURenderPass(pass);
    return rhi::FrameResult::presented;
}

void UnlitCubesLab::teardown(rhi::Host& host, [[maybe_unused]] render::Renderer& renderer)
{
    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(host.device(), pipeline_);
    if (vertices_) SDL_ReleaseGPUBuffer(host.device(), vertices_);
    if (indices_) SDL_ReleaseGPUBuffer(host.device(), indices_);
    pipeline_ = nullptr;
    vertices_ = indices_ = nullptr;
}

} // namespace forge::labs
