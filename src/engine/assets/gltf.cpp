#include "engine/assets/scene.hpp"

#include <cgltf.h>
#include <mikktspace.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace forge::assets {
namespace {
constexpr std::size_t max_vertices = 8'000'000;
constexpr std::size_t max_indices = 24'000'000;

void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

std::vector<float> unpack(const cgltf_accessor* accessor, std::size_t count, int components)
{
    require(accessor && accessor->count == count, "Attribute count does not match POSITION");
    require(cgltf_num_components(accessor->type) == static_cast<cgltf_size>(components), "Unexpected attribute type");
    std::vector<float> result(count * static_cast<std::size_t>(components));
    require(cgltf_accessor_unpack_floats(accessor, result.data(), result.size()) == result.size(), "Cannot decode accessor");
    require(std::all_of(result.begin(), result.end(), [](float v) { return std::isfinite(v); }), "Non-finite vertex attribute");
    return result;
}

const cgltf_accessor* attribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int index = 0)
{
    for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
        const auto& a = primitive.attributes[i];
        if (a.type == type && a.index == index) return a.data;
    }
    return nullptr;
}

TextureRef texture_ref(const cgltf_texture_view& view, const cgltf_data& data)
{
    TextureRef result;
    if (!view.texture) return result;
    const auto& texture = *view.texture;
    const auto* image = texture.has_basisu ? texture.basisu_image : texture.image;
    require(image, "Texture has no supported image source");
    result.image = static_cast<int>(image - data.images);
    if (texture.sampler) {
        const auto& s = *texture.sampler;
        if (s.min_filter) result.min_filter = s.min_filter;
        if (s.mag_filter) result.mag_filter = s.mag_filter;
        result.wrap_s = s.wrap_s;
        result.wrap_t = s.wrap_t;
    }
    int uv = view.texcoord;
    if (view.has_transform) {
        const auto& t = view.transform;
        if (t.has_texcoord) uv = t.texcoord;
        const float c = std::cos(t.rotation), s = std::sin(t.rotation);
        result.transform.u = {c * t.scale[0], -s * t.scale[1], t.offset[0], 0};
        result.transform.v = {s * t.scale[0], c * t.scale[1], t.offset[1], 0};
    }
    require(uv == 0 || uv == 1, "Only TEXCOORD_0 and TEXCOORD_1 are supported");
    result.transform.u.w = static_cast<float>(uv);
    return result;
}

Material material(const cgltf_material& source, const cgltf_data& data)
{
    require(source.alpha_mode != cgltf_alpha_mode_blend, "Alpha BLEND requires a transparent render pass (not implemented)");
    Material result;
    result.name = source.name ? source.name : "Material";
    const auto& pbr = source.pbr_metallic_roughness;
    if (source.has_pbr_metallic_roughness) {
        result.base_color_factor = glm::make_vec4(pbr.base_color_factor);
        result.metallic = pbr.metallic_factor;
        result.roughness = pbr.roughness_factor;
        result.textures[base_color] = texture_ref(pbr.base_color_texture, data);
        result.textures[metallic_roughness] = texture_ref(pbr.metallic_roughness_texture, data);
    }
    result.textures[normal] = texture_ref(source.normal_texture, data);
    result.textures[occlusion] = texture_ref(source.occlusion_texture, data);
    result.textures[emissive] = texture_ref(source.emissive_texture, data);
    result.normal_scale = source.normal_texture.scale;
    result.occlusion_strength = source.occlusion_texture.scale;
    result.emissive_factor = glm::make_vec3(source.emissive_factor)
        * (source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1.0f);
    result.alpha_mask = source.alpha_mode == cgltf_alpha_mode_mask;
    result.alpha_cutoff = source.alpha_cutoff;
    result.double_sided = source.double_sided;
    result.unlit = source.unlit;
    return result;
}

ImageSource image_source(const cgltf_image& image, const std::filesystem::path& base)
{
    ImageSource result;
    if (image.buffer_view) {
        const auto* bytes = cgltf_buffer_view_data(image.buffer_view);
        require(bytes && image.buffer_view->size <= 128 * 1024 * 1024, "Invalid embedded image");
        result.bytes.assign(bytes, bytes + image.buffer_view->size);
    } else if (image.uri && std::string_view(image.uri).starts_with("data:")) {
        const std::string_view uri(image.uri);
        const auto comma = uri.find(',');
        require(comma != std::string_view::npos && uri.substr(0, comma).ends_with(";base64"), "Image data URI must use base64");
        const auto payload = uri.substr(comma + 1);
        require(!payload.empty() && payload.size() % 4 == 0 && payload.size() <= 128 * 1024 * 1024, "Invalid image data URI size");
        const auto bytes = payload.size() / 4 * 3 - (payload.back() == '=') - (payload[payload.size()-2] == '=');
        cgltf_options options{};
        void* decoded = nullptr;
        require(cgltf_load_buffer_base64(&options, bytes, payload.data(), &decoded) == cgltf_result_success, "Invalid base64 image");
        std::unique_ptr<void, decltype(&std::free)> owned(decoded, std::free);
        const auto* start = static_cast<const std::uint8_t*>(decoded);
        result.bytes.assign(start, start + bytes);
    } else {
        require(image.uri, "Image has neither URI nor buffer view");
        std::string uri(image.uri);
        uri.resize(cgltf_decode_uri(uri.data()));
        require(uri.find("://") == std::string::npos, "Remote image URIs are not supported");
        result.path = base / std::filesystem::path(uri);
        auto cooked = result.path;
        cooked.replace_extension(".ktx2");
        if (std::filesystem::is_regular_file(cooked)) result.path = cooked;
        require(std::filesystem::is_regular_file(result.path), "Missing image: " + result.path.string());
    }
    return result;
}

struct Tangents {
    std::vector<Vertex>& vertices;
    const TextureRef& texture;
};
Vertex& vertex(const SMikkTSpaceContext* c, int face, int corner)
{
    return static_cast<Tangents*>(c->m_pUserData)->vertices[static_cast<std::size_t>(face) * 3 + corner];
}
void generate_tangents(std::vector<Vertex>& vertices, const TextureRef& texture)
{
    Tangents data{vertices, texture};
    SMikkTSpaceInterface api{};
    api.m_getNumFaces = [](const SMikkTSpaceContext* c) { return static_cast<int>(static_cast<Tangents*>(c->m_pUserData)->vertices.size() / 3); };
    api.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
    api.m_getPosition = [](const SMikkTSpaceContext* c, float out[], int f, int v) { std::memcpy(out, glm::value_ptr(vertex(c,f,v).position), 12); };
    api.m_getNormal = [](const SMikkTSpaceContext* c, float out[], int f, int v) { std::memcpy(out, glm::value_ptr(vertex(c,f,v).normal), 12); };
    api.m_getTexCoord = [](const SMikkTSpaceContext* c, float out[], int f, int v) {
        const auto& t = static_cast<Tangents*>(c->m_pUserData)->texture.transform;
        const auto& p = vertex(c,f,v);
        const auto uv = t.u.w > 0.5f ? p.uv1 : p.uv0;
        out[0] = glm::dot(glm::vec3(t.u), glm::vec3(uv,1));
        out[1] = glm::dot(glm::vec3(t.v), glm::vec3(uv,1));
    };
    api.m_setTSpaceBasic = [](const SMikkTSpaceContext* c, const float t[], float sign, int f, int v) {
        vertex(c,f,v).tangent = {t[0],t[1],t[2],sign};
    };
    SMikkTSpaceContext context{&api, &data};
    require(genTangSpaceDefault(&context) != 0, "MikkTSpace tangent generation failed");
}

struct VertexHash {
    std::size_t operator()(const std::array<std::uint32_t, 26>& words) const {
        std::size_t hash = 1469598103934665603ull;
        for (auto word : words) hash = (hash ^ word) * 1099511628211ull;
        return hash;
    }
};

void append_primitive(Scene& scene, const cgltf_primitive& primitive, const cgltf_data& data, const glm::mat4& transform,
                      int skin)
{
    require(primitive.type == cgltf_primitive_type_triangles, "Only triangle-list primitives are supported");
    require(!primitive.has_draco_mesh_compression && primitive.targets_count == 0, "Draco and morph targets are not supported yet");
    const auto* positions = attribute(primitive, cgltf_attribute_type_position);
    require(positions && positions->count > 0 && positions->count <= max_vertices, "Missing or oversized POSITION accessor");
    const auto count = positions->count;
    std::vector<Vertex> source(count);
    const auto p = unpack(positions, count, 3);
    for (std::size_t i=0; i<count; ++i) source[i].position = glm::make_vec3(p.data()+i*3);
    const auto* normals = attribute(primitive, cgltf_attribute_type_normal);
    if (normals) {
        const auto n = unpack(normals, count, 3);
        for (std::size_t i=0; i<count; ++i) {
            const auto v = glm::make_vec3(n.data()+i*3);
            require(glm::dot(v,v) > 1e-12f, "Zero-length vertex normal");
            source[i].normal = glm::normalize(v);
        }
    }
    const auto* tangents = attribute(primitive, cgltf_attribute_type_tangent);
    if (tangents) {
        const auto t = unpack(tangents, count, 4);
        for (std::size_t i=0; i<count; ++i) source[i].tangent = glm::make_vec4(t.data()+i*4);
    }
    for (int set=0; set<2; ++set) {
        if (const auto* uv = attribute(primitive, cgltf_attribute_type_texcoord, set)) {
            const auto values = unpack(uv, count, 2);
            for (std::size_t i=0; i<count; ++i) (set == 0 ? source[i].uv0 : source[i].uv1) = glm::make_vec2(values.data()+i*2);
        }
    }
    if (const auto* color = attribute(primitive, cgltf_attribute_type_color)) {
        const int components = static_cast<int>(cgltf_num_components(color->type));
        require(components == 3 || components == 4, "COLOR_0 must be vec3 or vec4");
        const auto colors = unpack(color, count, components);
        for (std::size_t i=0; i<count; ++i) {
            source[i].color = glm::vec4(glm::make_vec3(colors.data()+i*components), components == 4 ? colors[i*4+3] : 1.0f);
        }
    }
    if (skin >= 0) {
        const auto* joints = attribute(primitive, cgltf_attribute_type_joints);
        const auto* weights = attribute(primitive, cgltf_attribute_type_weights);
        require(joints && weights, "Skinned primitive needs JOINTS_0 and WEIGHTS_0");
        const auto joint_values = unpack(joints, count, 4);
        const auto weight_values = unpack(weights, count, 4);
        for (std::size_t i = 0; i < count; ++i) {
            source[i].joints = glm::make_vec4(joint_values.data() + i * 4);
            source[i].weights = glm::make_vec4(weight_values.data() + i * 4);
            const float sum = source[i].weights.x + source[i].weights.y + source[i].weights.z + source[i].weights.w;
            if (sum > 1e-8f) source[i].weights /= sum;
            require(source[i].joints.x >= 0 && source[i].joints.x < kMaxJoints
                        && source[i].joints.y >= 0 && source[i].joints.y < kMaxJoints
                        && source[i].joints.z >= 0 && source[i].joints.z < kMaxJoints
                        && source[i].joints.w >= 0 && source[i].joints.w < kMaxJoints,
                    "Joint index exceeds kMaxJoints");
        }
    }
    const auto material_index = primitive.material
        ? static_cast<std::uint32_t>(primitive.material - data.materials)
        : static_cast<std::uint32_t>(scene.materials.size() - 1);
    const auto& mat = scene.materials[material_index];
    for (const auto& texture : mat.textures) {
        if (texture.image >= 0) require(attribute(primitive, cgltf_attribute_type_texcoord, static_cast<int>(texture.transform.u.w)), "Material references a missing UV set");
    }
    const auto index_count = primitive.indices ? primitive.indices->count : count;
    require(index_count > 0 && index_count % 3 == 0 && index_count <= max_indices, "Invalid triangle index count");
    std::vector<std::uint32_t> indices(index_count);
    if (primitive.indices) {
        require(!primitive.indices->is_sparse, "Sparse index accessors are not supported");
        require(cgltf_accessor_unpack_indices(primitive.indices, indices.data(), sizeof(std::uint32_t), index_count) == index_count, "Cannot decode index accessor");
    } else std::iota(indices.begin(), indices.end(), 0u);
    std::vector<Vertex> corners;
    corners.reserve(index_count);
    for (auto index : indices) {
        require(index < count, "Index exceeds vertex count");
        corners.push_back(source[index]);
    }
    if (!normals) {
        for (std::size_t i=0; i<corners.size(); i+=3) {
            auto n = glm::cross(corners[i+1].position-corners[i].position, corners[i+2].position-corners[i].position);
            n = glm::dot(n,n) > 1e-16f ? glm::normalize(n) : glm::vec3(0,1,0);
            for (std::size_t j=0; j<3; ++j) corners[i+j].normal = n;
        }
    }
    if (!tangents && mat.textures[normal].image >= 0) generate_tangents(corners, mat.textures[normal]);
    const glm::mat3 linear(transform);
    const float determinant = glm::determinant(linear);
    require(std::isfinite(determinant) && std::abs(determinant) > 1e-12f, "Singular node transform");
    const glm::mat3 normal_matrix = glm::inverseTranspose(linear);
    if (determinant < 0) for (std::size_t i=0; i<corners.size(); i+=3) std::swap(corners[i+1], corners[i+2]);
    Primitive draw{static_cast<std::uint32_t>(scene.indices.size()), static_cast<std::uint32_t>(corners.size()),
                   material_index, skin};
    std::unordered_map<std::array<std::uint32_t, 26>, std::uint32_t, VertexHash> unique;
    for (auto& v : corners) {
        v.position = glm::vec3(transform * glm::vec4(v.position, 1));
        v.normal = glm::normalize(normal_matrix * v.normal);
        auto tangent = linear * glm::vec3(v.tangent);
        tangent -= v.normal * glm::dot(v.normal, tangent);
        if (glm::dot(tangent,tangent) < 1e-12f) tangent = glm::cross(std::abs(v.normal.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0), v.normal);
        v.tangent = glm::vec4(glm::normalize(tangent), v.tangent.w * (determinant < 0 ? -1.0f : 1.0f));
        const auto words = std::bit_cast<std::array<std::uint32_t, 26>>(v);
        auto [entry, added] = unique.emplace(words, static_cast<std::uint32_t>(scene.vertices.size()));
        if (added) scene.vertices.push_back(v);
        scene.indices.push_back(entry->second);
    }
    scene.primitives.push_back(draw);
    require(scene.vertices.size() <= max_vertices && scene.indices.size() <= max_indices, "Scene exceeds geometry budget");
}

} // namespace

bool load_gltf(const std::filesystem::path& path, Scene& destination, std::string& error)
{
    try {
        cgltf_options options{};
        cgltf_data* parsed = nullptr;
        const auto result = cgltf_parse_file(&options, path.string().c_str(), &parsed);
        require(result == cgltf_result_success, "cgltf parse failed (" + std::to_string(result) + "): " + path.string());
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, cgltf_free);
        const std::unordered_set<std::string> extensions{"KHR_texture_basisu", "KHR_texture_transform", "KHR_materials_unlit", "KHR_materials_emissive_strength", "KHR_mesh_quantization"};
        for (std::size_t i=0; i<data->extensions_required_count; ++i)
            require(extensions.contains(data->extensions_required[i]), "Unsupported required extension: " + std::string(data->extensions_required[i]));
        for (std::size_t i=0; i<data->buffer_views_count; ++i) require(!data->buffer_views[i].has_meshopt_compression, "Meshopt compressed buffers require a decoder");
        require(cgltf_load_buffers(&options, data.get(), path.string().c_str()) == cgltf_result_success, "cgltf cannot load buffers");
        require(cgltf_validate(data.get()) == cgltf_result_success, "cgltf validation failed");
        Scene scene;
        scene.materials.resize(data->materials_count + 1);
        for (std::size_t i = 0; i < data->materials_count; ++i) {
            scene.materials[i] = material(data->materials[i], *data);
            if (data->materials[i].has_transmission) {
                scene.warnings.push_back("KHR_materials_transmission on '" + scene.materials[i].name
                                         + "' is ignored (rendered opaque)");
            }
        }
        scene.images.resize(data->images_count);
        for (std::size_t i = 0; i < data->extensions_used_count; ++i) {
            if (!extensions.contains(data->extensions_used[i])) {
                scene.warnings.push_back("Using core glTF fallback for optional extension: "
                                         + std::string(data->extensions_used[i]));
            }
        }
        scene.nodes.resize(data->nodes_count);
        for (std::size_t i = 0; i < data->nodes_count; ++i) {
            const auto& source = data->nodes[i];
            auto& node = scene.nodes[i];
            node.name = source.name ? source.name : ("node" + std::to_string(i));
            node.parent = source.parent ? static_cast<int>(source.parent - data->nodes) : -1;
            require(node.parent >= -1 && node.parent < static_cast<int>(data->nodes_count) && node.parent != static_cast<int>(i),
                    "Invalid node parent");
            if (source.has_translation) node.translation = glm::make_vec3(source.translation);
            if (source.has_scale) node.scale = glm::make_vec3(source.scale);
            if (source.has_rotation)
                node.rotation = glm::quat(source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]);
            if (source.has_matrix && !source.has_translation && !source.has_rotation && !source.has_scale) {
                const glm::mat4 matrix = glm::make_mat4(source.matrix);
                node.translation = glm::vec3(matrix[3]);
                node.scale = {glm::length(glm::vec3(matrix[0])), glm::length(glm::vec3(matrix[1])),
                              glm::length(glm::vec3(matrix[2]))};
                glm::mat3 rot(matrix);
                if (node.scale.x > 1e-8f) rot[0] /= node.scale.x;
                if (node.scale.y > 1e-8f) rot[1] /= node.scale.y;
                if (node.scale.z > 1e-8f) rot[2] /= node.scale.z;
                node.rotation = glm::normalize(glm::quat_cast(rot));
            }
        }

        scene.skins.resize(data->skins_count);
        for (std::size_t i = 0; i < data->skins_count; ++i) {
            const auto& source = data->skins[i];
            auto& skin = scene.skins[i];
            skin.name = source.name ? source.name : ("skin" + std::to_string(i));
            require(source.joints_count > 0 && source.joints_count <= static_cast<cgltf_size>(kMaxJoints),
                    "Skin joint count must be 1..48");
            skin.joints.resize(source.joints_count);
            for (std::size_t j = 0; j < source.joints_count; ++j) {
                require(source.joints[j], "Skin joint is null");
                skin.joints[j] = static_cast<int>(source.joints[j] - data->nodes);
                require(skin.joints[j] >= 0 && skin.joints[j] < static_cast<int>(data->nodes_count), "Skin joint out of range");
            }
            if (source.inverse_bind_matrices) {
                require(source.inverse_bind_matrices->count == source.joints_count, "inverseBindMatrices count must match joints");
                const auto ibm = unpack(source.inverse_bind_matrices, source.joints_count, 16);
                skin.inverse_bind.resize(source.joints_count);
                for (std::size_t j = 0; j < source.joints_count; ++j)
                    skin.inverse_bind[j] = glm::make_mat4(ibm.data() + j * 16);
            } else {
                skin.inverse_bind.assign(source.joints_count, glm::mat4(1.0f));
            }
        }

        scene.animations.resize(data->animations_count);
        for (std::size_t i = 0; i < data->animations_count; ++i) {
            const auto& source = data->animations[i];
            auto& clip = scene.animations[i];
            clip.name = source.name ? source.name : ("clip" + std::to_string(i));
            for (std::size_t c = 0; c < source.channels_count; ++c) {
                const auto& channel = source.channels[c];
                if (!channel.target_node || !channel.sampler) continue;
                if (channel.target_path == cgltf_animation_path_type_weights) {
                    scene.warnings.push_back("Morph-target weights animation on '" + clip.name + "' is ignored");
                    continue;
                }
                require(channel.target_path == cgltf_animation_path_type_translation
                            || channel.target_path == cgltf_animation_path_type_rotation
                            || channel.target_path == cgltf_animation_path_type_scale,
                        "Unsupported animation path");
                const auto* input = channel.sampler->input;
                const auto* output = channel.sampler->output;
                require(input && output && input->count > 0, "Animation sampler missing accessors");
                AnimChannel sampled;
                sampled.node = static_cast<int>(channel.target_node - data->nodes);
                sampled.path = channel.target_path == cgltf_animation_path_type_rotation ? AnimPath::Rotation
                    : channel.target_path == cgltf_animation_path_type_scale ? AnimPath::Scale
                                                                            : AnimPath::Translation;
                sampled.interp = channel.sampler->interpolation == cgltf_interpolation_type_step ? AnimInterp::Step
                                                                                                 : AnimInterp::Linear;
                if (channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline)
                    scene.warnings.push_back("CUBICSPLINE animation on '" + clip.name + "' is sampled as LINEAR");
                sampled.times = unpack(input, input->count, 1);
                const int components = sampled.path == AnimPath::Rotation ? 4 : 3;
                const bool cubic = channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline;
                const auto raw = unpack(output, output->count, components);
                if (cubic) {
                    require(output->count == input->count * 3, "CUBICSPLINE output count");
                    sampled.values.resize(static_cast<std::size_t>(input->count) * static_cast<std::size_t>(components));
                    for (std::size_t k = 0; k < input->count; ++k) {
                        const auto* src = raw.data() + (k * 3 + 1) * components;
                        std::copy(src, src + components, sampled.values.data() + k * components);
                    }
                } else {
                    require(output->count == input->count, "Animation output count must match input");
                    sampled.values = raw;
                }
                if (!sampled.times.empty()) clip.duration = std::max(clip.duration, sampled.times.back());
                clip.channels.push_back(std::move(sampled));
            }
        }

        struct Visit { const cgltf_node* node; glm::mat4 parent; };
        std::vector<Visit> pending;
        const auto* selected = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
        if (selected) for (std::size_t i=0; i<selected->nodes_count; ++i) pending.push_back({selected->nodes[i], glm::mat4(1)});
        else for (std::size_t i=0; i<data->nodes_count; ++i) if (!data->nodes[i].parent) pending.push_back({&data->nodes[i], glm::mat4(1)});
        std::unordered_set<const cgltf_node*> visited;
        while (!pending.empty()) {
            const auto visit = pending.back(); pending.pop_back();
            require(visited.insert(visit.node).second, "Cycle or duplicate node in scene");
            glm::mat4 local;
            cgltf_node_transform_local(visit.node, glm::value_ptr(local));
            const auto world = visit.parent * local;
            for (int col=0; col<4; ++col) for (int row=0; row<4; ++row) require(std::isfinite(world[col][row]), "Non-finite node transform");
            if (visit.node->mesh) {
                const int skin = visit.node->skin ? static_cast<int>(visit.node->skin - data->skins) : -1;
                const glm::mat4 mesh_transform = skin >= 0 ? glm::mat4(1.0f) : world;
                for (std::size_t i=0; i<visit.node->mesh->primitives_count; ++i)
                    append_primitive(scene, visit.node->mesh->primitives[i], *data, mesh_transform, skin);
            }
            for (std::size_t i=0; i<visit.node->children_count; ++i) pending.push_back({visit.node->children[i], world});
        }
        require(!scene.vertices.empty(), "Scene contains no triangle geometry");
        std::vector<bool> loaded_images(data->images_count);
        for (const auto& mat : scene.materials) for (const auto& texture : mat.textures) {
            if (texture.image >= 0 && !loaded_images[texture.image]) {
                scene.images[texture.image] = image_source(data->images[texture.image], path.parent_path());
                loaded_images[texture.image] = true;
            }
        }
        scene.bounds_min = scene.bounds_max = scene.vertices[0].position;
        for (const auto& vertex : scene.vertices) {
            scene.bounds_min = glm::min(scene.bounds_min, vertex.position);
            scene.bounds_max = glm::max(scene.bounds_max, vertex.position);
        }
        destination = std::move(scene);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

} // namespace forge::assets
