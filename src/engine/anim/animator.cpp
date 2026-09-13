#include "engine/anim/animator.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace forge::anim {
namespace {

glm::vec3 sample_vec3(const assets::AnimChannel& channel, float time)
{
    const auto& times = channel.times;
    if (times.empty()) return {};
    if (time <= times.front()) return glm::make_vec3(channel.values.data());
    if (time >= times.back())
        return glm::make_vec3(channel.values.data() + (times.size() - 1) * 3);
    const auto next = std::upper_bound(times.begin(), times.end(), time);
    const auto i1 = static_cast<std::size_t>(next - times.begin());
    const auto i0 = i1 - 1;
    if (channel.interp == assets::AnimInterp::Step) return glm::make_vec3(channel.values.data() + i0 * 3);
    const float span = times[i1] - times[i0];
    const float t = span > 1e-8f ? (time - times[i0]) / span : 0.0f;
    return glm::mix(glm::make_vec3(channel.values.data() + i0 * 3), glm::make_vec3(channel.values.data() + i1 * 3), t);
}

glm::quat sample_quat(const assets::AnimChannel& channel, float time)
{
    const auto load = [&](std::size_t index) {
        const float* v = channel.values.data() + index * 4;
        return glm::normalize(glm::quat(v[3], v[0], v[1], v[2]));
    };
    const auto& times = channel.times;
    if (times.empty()) return glm::quat(1, 0, 0, 0);
    if (time <= times.front()) return load(0);
    if (time >= times.back()) return load(times.size() - 1);
    const auto next = std::upper_bound(times.begin(), times.end(), time);
    const auto i1 = static_cast<std::size_t>(next - times.begin());
    const auto i0 = i1 - 1;
    if (channel.interp == assets::AnimInterp::Step) return load(i0);
    const float span = times[i1] - times[i0];
    const float t = span > 1e-8f ? (time - times[i0]) / span : 0.0f;
    auto a = load(i0);
    auto b = load(i1);
    if (glm::dot(a, b) < 0.0f) b = -b;
    return glm::normalize(glm::slerp(a, b, t));
}

float wrap_time(float time, float duration)
{
    if (duration <= 1e-6f) return 0.0f;
    time = std::fmod(time, duration);
    if (time < 0.0f) time += duration;
    return time;
}

} // namespace

int find_clip(const assets::Scene& scene, std::string_view name)
{
    for (int i = 0; i < static_cast<int>(scene.animations.size()); ++i) {
        if (scene.animations[static_cast<std::size_t>(i)].name == name) return i;
    }
    return scene.animations.empty() ? -1 : 0;
}

void rest_pose(const assets::Scene& scene, std::vector<glm::vec3>& translation, std::vector<glm::quat>& rotation,
               std::vector<glm::vec3>& scale)
{
    translation.resize(scene.nodes.size());
    rotation.resize(scene.nodes.size());
    scale.resize(scene.nodes.size());
    for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
        translation[i] = scene.nodes[i].translation;
        rotation[i] = scene.nodes[i].rotation;
        scale[i] = scene.nodes[i].scale;
    }
}

void sample_clip(const assets::Scene& scene, int clip, float time, std::vector<glm::vec3>& translation,
                 std::vector<glm::quat>& rotation, std::vector<glm::vec3>& scale)
{
    rest_pose(scene, translation, rotation, scale);
    if (clip < 0 || clip >= static_cast<int>(scene.animations.size())) return;
    const auto& animation = scene.animations[static_cast<std::size_t>(clip)];
    const float t = wrap_time(time, animation.duration);
    for (const auto& channel : animation.channels) {
        if (channel.node < 0 || channel.node >= static_cast<int>(scene.nodes.size())) continue;
        const auto index = static_cast<std::size_t>(channel.node);
        if (channel.path == assets::AnimPath::Translation) translation[index] = sample_vec3(channel, t);
        else if (channel.path == assets::AnimPath::Scale) scale[index] = sample_vec3(channel, t);
        else rotation[index] = sample_quat(channel, t);
    }
}

void compute_globals(const assets::Scene& scene, const std::vector<glm::vec3>& translation,
                     const std::vector<glm::quat>& rotation, const std::vector<glm::vec3>& scale,
                     std::vector<glm::mat4>& globals)
{
    globals.assign(scene.nodes.size(), glm::mat4(1.0f));
    std::vector<char> done(scene.nodes.size(), 0);
    std::vector<char> visiting(scene.nodes.size(), 0);
    const auto local = [&](std::size_t i) {
        return glm::translate(glm::mat4(1.0f), translation[i]) * glm::mat4_cast(rotation[i])
            * glm::scale(glm::mat4(1.0f), scale[i]);
    };
    const auto compute = [&](auto&& self, int index) -> void {
        if (index < 0 || done[static_cast<std::size_t>(index)]) return;
        if (visiting[static_cast<std::size_t>(index)]) {
            done[static_cast<std::size_t>(index)] = 1;
            return;
        }
        visiting[static_cast<std::size_t>(index)] = 1;
        const int parent = scene.nodes[static_cast<std::size_t>(index)].parent;
        if (parent >= 0) self(self, parent);
        const glm::mat4 parent_global =
            parent >= 0 ? globals[static_cast<std::size_t>(parent)] : glm::mat4(1.0f);
        globals[static_cast<std::size_t>(index)] = parent_global * local(static_cast<std::size_t>(index));
        visiting[static_cast<std::size_t>(index)] = 0;
        done[static_cast<std::size_t>(index)] = 1;
    };
    for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) compute(compute, i);
}

bool compute_palette(const assets::Scene& scene, int skin, const std::vector<glm::mat4>& globals, Palette& out)
{
    out = {};
    if (skin < 0 || skin >= static_cast<int>(scene.skins.size())) return false;
    const auto& source = scene.skins[static_cast<std::size_t>(skin)];
    if (source.joints.empty() || source.joints.size() > static_cast<std::size_t>(assets::kMaxJoints)) return false;
    out.count = static_cast<int>(source.joints.size());
    for (int i = 0; i < assets::kMaxJoints; ++i) out.joints[i] = glm::mat4(1.0f);
    for (int j = 0; j < out.count; ++j) {
        const int node = source.joints[static_cast<std::size_t>(j)];
        if (node < 0 || node >= static_cast<int>(globals.size())) return false;
        out.joints[j] = globals[static_cast<std::size_t>(node)] * source.inverse_bind[static_cast<std::size_t>(j)];
    }
    return true;
}

bool evaluate(const assets::Scene& scene, int skin, int clip, float time, Palette& out)
{
    std::vector<glm::vec3> translation, scale;
    std::vector<glm::quat> rotation;
    std::vector<glm::mat4> globals;
    sample_clip(scene, clip, time, translation, rotation, scale);
    compute_globals(scene, translation, rotation, scale, globals);
    return compute_palette(scene, skin, globals, out);
}

} // namespace forge::anim
