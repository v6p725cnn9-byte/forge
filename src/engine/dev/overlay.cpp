#include "engine/dev/overlay.hpp"

#include "engine/app/i18n.hpp"
#include "engine/core/paths.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <filesystem>
#include <string_view>

namespace forge::dev {
namespace {

bool add_cyrillic_font(float pixel_size)
{
    auto& io = ImGui::GetIO();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    const std::filesystem::path candidates[] = {
        forge::assets_directory() / "fonts/DroidSans.ttf",
        forge::executable_directory() / "assets/fonts/DroidSans.ttf",
        std::filesystem::current_path() / "assets/fonts/DroidSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial Unicode.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const auto& path : candidates) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) continue;
        io.Fonts->Clear();
        if (io.Fonts->AddFontFromFileTTF(path.string().c_str(), pixel_size, nullptr, ranges)) {
            SDL_Log("UI font: %s (Latin+Cyrillic)", path.string().c_str());
            return true;
        }
    }
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();
    SDL_Log("UI font: default (no Cyrillic). Stage assets/fonts/DroidSans.ttf");
    return false;
}

} // namespace

bool Overlay::init(SDL_Window* window, SDL_GPUDevice* device)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    const float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
    ImGui::GetStyle().ScaleAllSizes(scale);
    add_cyrillic_font(18.0f * (scale > 0.1f ? scale : 1.0f));
    if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
        ImGui::DestroyContext();
        return false;
    }
    ImGui_ImplSDLGPU3_InitInfo info{};
    info.Device = device;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    initialized_ = true;
    return true;
}

void Overlay::shutdown()
{
    if (!initialized_) return;
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void Overlay::process_event(const SDL_Event& event)
{
    if (initialized_) ImGui_ImplSDL3_ProcessEvent(&event);
}

bool Overlay::wants_mouse() const { return initialized_ && ImGui::GetIO().WantCaptureMouse; }
bool Overlay::wants_keyboard() const { return initialized_ && ImGui::GetIO().WantCaptureKeyboard; }

void Overlay::begin_frame()
{
    if (!initialized_ || frame_open_) return;
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    frame_open_ = true;
}

void Overlay::end_frame()
{
    if (!initialized_ || !frame_open_) return;
    ImGui::Render();
    frame_open_ = false;
}

void Overlay::draw_hud(const forge::render::DebugState& debug)
{
    if (!initialized_ || !debug.survival) return;
    const char* lang = debug.language ? debug.language : "ru";
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 16), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##hud", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("%s %.0f    %s %.0f", forge::app::tr(lang, "hp"), debug.hp, forge::app::tr(lang, "cold"), debug.cold);
    ImGui::Text("%s %u    %s %u", forge::app::tr(lang, "wood"), debug.wood, forge::app::tr(lang, "stone"), debug.stone);
    const char* phase = debug.phase == 1 ? "hud_extracted" : (debug.phase == 2 ? "hud_failed" : "hud_drop");
    ImGui::Text("%s  %u s  |  %s  |  %s", forge::app::tr(lang, "hud_session"), debug.time_left,
                debug.night ? forge::app::tr(lang, "hud_night") : forge::app::tr(lang, "hud_day"),
                forge::app::tr(lang, phase));
    if (debug.join_hint[0] && debug.net_role && std::string_view(debug.net_role) == "host")
        ImGui::TextUnformatted(debug.join_hint);
    ImGui::TextDisabled("%s", forge::app::tr(lang, "hud_help"));
    ImGui::End();
}

void Overlay::draw_debug(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width,
                         Uint32 height, Uint32 triangles, bool captured)
{
#if !FORGE_DEV_UI
    (void)camera;
    (void)debug;
    (void)backend;
    (void)width;
    (void)height;
    (void)triangles;
    (void)captured;
    return;
#else
    if (!initialized_ || !visible_) return;
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.94f);
    const auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse
        | (captured ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None);
    if (ImGui::Begin("Forge lab", nullptr, flags)) {
        ImGui::Text("%s  |  %s  |  %u x %u px  |  %u tris", debug.lab, backend, width, height, triangles);
        ImGui::Text("%s  |  %u materials  |  %u textures", debug.model, debug.materials, debug.textures);
        const auto& io = ImGui::GetIO();
        ImGui::Text("%.1f fps  /  %.2f ms", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Separator();
        ImGui::Text("Position   %.2f  %.2f  %.2f", camera.position.x, camera.position.y, camera.position.z);
        ImGui::SliderFloat("Speed", &camera.speed, 0.1f, 80.0f, "%.2f m/s");
        ImGui::SliderFloat("Field of view", &camera.vertical_fov, 35.0f, 100.0f, "%.0f deg");
        ImGui::SliderFloat("Exposure", &debug.exposure, 0.2f, 3.0f, "%.2f");
        if (debug.survival) {
            ImGui::Text("Net  %s  |  %u in world  |  %.0f ms", debug.net_role, debug.net_peers, debug.net_ping_ms);
        } else if (debug.net_role && debug.net_role[0]) {
            ImGui::Text("Net  %s  |  tick %u  |  %u peers  |  %.0f ms", debug.net_role, debug.net_tick, debug.net_peers,
                        debug.net_ping_ms);
            ImGui::SliderFloat("Stream radius", &debug.stream_radius, 16.0f, 160.0f, "%.0f m");
        } else if (debug.stream_radius > 0.0f) {
            ImGui::SliderFloat("Stream radius", &debug.stream_radius, 32.0f, 192.0f, "%.0f m");
            ImGui::Checkbox("Frustum cull", &debug.frustum_cull);
        }
        if (ImGui::Button("Reset camera")) {
            camera.position = debug.home_position;
            camera.yaw = debug.home_yaw;
            camera.pitch = debug.home_pitch;
        }
        if (debug.help && debug.help[0]) ImGui::TextUnformatted(debug.help);
    }
    ImGui::End();
#endif
}

void Overlay::draw_world_labels(const Camera& camera, const forge::render::DebugState& debug, Uint32 width,
                                Uint32 height)
{
    if (!initialized_ || debug.world_label_count == 0) return;
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 clip_from_world = camera.projection(aspect) * camera.view();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    auto* draw = ImGui::GetBackgroundDrawList();
    for (std::uint32_t i = 0; i < debug.world_label_count; ++i) {
        const char* text = debug.world_label_text[i];
        if (!text || !text[0]) continue;
        const glm::vec4 clip = clip_from_world * glm::vec4(debug.world_label_pos[i], 1.0f);
        if (clip.w <= 0.15f) continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < 0.0f || ndc.z > 1.0f || ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f) continue;
        const ImVec2 pos{(ndc.x * 0.5f + 0.5f) * display.x, (1.0f - (ndc.y * 0.5f + 0.5f)) * display.y};
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 aligned{pos.x - size.x * 0.5f, pos.y - size.y};
        draw->AddText(ImVec2(aligned.x + 1.0f, aligned.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
        draw->AddText(aligned, IM_COL32(240, 240, 245, 255), text);
    }
}

void Overlay::build(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width, Uint32 height,
                    Uint32 triangles, bool captured)
{
    if (!initialized_) return;
    begin_frame();
    draw_debug(camera, debug, backend, width, height, triangles, captured);
    draw_world_labels(camera, debug, width, height);
    end_frame();
}

void Overlay::prepare(SDL_GPUCommandBuffer* command)
{
    if (initialized_) ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), command);
}

void Overlay::render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass)
{
    if (initialized_) ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), command, pass);
}

} // namespace forge::dev
