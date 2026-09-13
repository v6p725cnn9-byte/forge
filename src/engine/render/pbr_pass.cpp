#include "engine/render/pbr_pass.hpp"

#include "engine/assets/scene.hpp"
#include "engine/rhi/shader.hpp"
#include "engine/world/types.hpp"

#include <cstddef>

namespace forge::render {

SDL_GPUGraphicsPipeline* make_pbr_pipeline(rhi::Host& host, bool double_sided)
{
    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 9});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        return nullptr;
    }
    SDL_GPUVertexBufferDescription vertex_buffer{};
    vertex_buffer.pitch = sizeof(assets::Vertex);
    vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, normal))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, tangent))},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv0))},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv1))},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, color))},
    };
    SDL_GPUColorTargetDescription color{};
    color.format = host.hdr_format();
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.num_vertex_attributes = 6;
    info.vertex_input_state.vertex_attributes = attributes;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = double_sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = host.depth_format();
    auto* pipeline = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline;
}

SDL_GPUGraphicsPipeline* make_pbr_instanced_pipeline(rhi::Host& host, bool double_sided)
{
    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr_instanced.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 9});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        return nullptr;
    }
    SDL_GPUVertexBufferDescription buffers[2]{};
    buffers[0].slot = 0;
    buffers[0].pitch = sizeof(assets::Vertex);
    buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    buffers[1].slot = 1;
    buffers[1].pitch = sizeof(world::GpuInstance);
    buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, normal))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, tangent))},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv0))},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv1))},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, color))},
        {6, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
        {7, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
        {8, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
        {9, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
        {10, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
    };
    SDL_GPUColorTargetDescription color{};
    color.format = host.hdr_format();
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = buffers;
    info.vertex_input_state.num_vertex_buffers = 2;
    info.vertex_input_state.num_vertex_attributes = 11;
    info.vertex_input_state.vertex_attributes = attributes;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = double_sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = host.depth_format();
    auto* pipeline = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline;
}

SDL_GPUGraphicsPipeline* make_pbr_skinned_pipeline(rhi::Host& host, bool double_sided)
{
    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr_skinned.vert", SDL_GPU_SHADERSTAGE_VERTEX, 2});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"pbr.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 9});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        return nullptr;
    }
    SDL_GPUVertexBufferDescription vertex_buffer{};
    vertex_buffer.pitch = sizeof(assets::Vertex);
    vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, normal))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, tangent))},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv0))},
        {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(assets::Vertex, uv1))},
        {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, color))},
        {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, joints))},
        {7, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, static_cast<Uint32>(offsetof(assets::Vertex, weights))},
    };
    SDL_GPUColorTargetDescription color{};
    color.format = host.hdr_format();
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.num_vertex_attributes = 8;
    info.vertex_input_state.vertex_attributes = attributes;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = double_sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = host.depth_format();
    auto* pipeline = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline;
}

SDL_GPUGraphicsPipeline* make_shadow_pipeline(rhi::Host& host)
{
    auto* vs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"shadow.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1});
    auto* fs = rhi::load_shader(host.device(), rhi::shader_directory(),
                                {"shadow.frag", SDL_GPU_SHADERSTAGE_FRAGMENT});
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(host.device(), vs);
        if (fs) SDL_ReleaseGPUShader(host.device(), fs);
        return nullptr;
    }
    SDL_GPUVertexBufferDescription vertex_buffer{};
    vertex_buffer.pitch = sizeof(assets::Vertex);
    vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, static_cast<Uint32>(offsetof(assets::Vertex, position))},
    };
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.num_vertex_attributes = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = false;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = host.depth_format();
    auto* pipeline = SDL_CreateGPUGraphicsPipeline(host.device(), &info);
    SDL_ReleaseGPUShader(host.device(), vs);
    SDL_ReleaseGPUShader(host.device(), fs);
    return pipeline;
}

void apply_bloom(rhi::Host& host, rhi::Command& command, float threshold)
{
    if (!host.bloom() || !host.bloom_pipeline()) return;
    SDL_GPUColorTargetInfo color{};
    color.texture = host.bloom();
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return;
    SDL_BindGPUGraphicsPipeline(pass, host.bloom_pipeline());
    const SDL_GPUTextureSamplerBinding binding{host.hdr(), host.linear_clamp()};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const glm::vec4 params{threshold, 0.1f, 0.0f, 0.0f};
    SDL_PushGPUFragmentUniformData(command.handle, 0, &params, sizeof(params));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

void apply_tonemap(rhi::Host& host, rhi::Command& command, SDL_GPUTexture* swapchain, float exposure,
                   float bloom_strength)
{
    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command.handle, &color, 1, nullptr);
    if (!pass) return;
    SDL_BindGPUGraphicsPipeline(pass, host.tonemap_pipeline());
    SDL_GPUTexture* bloom = host.bloom() ? host.bloom() : host.hdr();
    const SDL_GPUTextureSamplerBinding bindings[2] = {
        {host.hdr(), host.linear_clamp()},
        {bloom, host.linear_clamp()},
    };
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);
    const glm::vec4 exposure_bloom{exposure, bloom_strength, 0, 0};
    SDL_PushGPUFragmentUniformData(command.handle, 0, &exposure_bloom, sizeof(exposure_bloom));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

} // namespace forge::render
