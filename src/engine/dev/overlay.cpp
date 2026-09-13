#include "engine/dev/overlay.hpp"

#if FORGE_DEV_UI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#endif

namespace forge::dev {

#if FORGE_DEV_UI
bool Overlay::init(SDL_Window* window, SDL_GPUDevice* device)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    // The SDL backend already applies Retina pixel density to the framebuffer.
    const float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;
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

bool Overlay::wants_mouse() const { return initialized_ && visible_ && ImGui::GetIO().WantCaptureMouse; }
bool Overlay::wants_keyboard() const { return initialized_ && visible_ && ImGui::GetIO().WantCaptureKeyboard; }

void Overlay::build(Camera& camera, forge::render::DebugState& debug, const char* backend, Uint32 width, Uint32 height,
                    Uint32 triangles, bool captured)
{
    if (!initialized_) return;
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (visible_) {
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
            ImGui::Text("Yaw / pitch   %.1f / %.1f", camera.yaw, camera.pitch);
            ImGui::SliderFloat("Speed", &camera.speed, 0.1f, 80.0f, "%.2f m/s");
            ImGui::SliderFloat("Field of view", &camera.vertical_fov, 35.0f, 100.0f, "%.0f deg");
            ImGui::SliderFloat("Exposure", &debug.exposure, 0.2f, 3.0f, "%.2f");
            ImGui::SliderFloat("IBL", &debug.ibl_intensity, 0.0f, 2.5f, "%.2f");
            ImGui::SliderFloat("Sun intensity", &debug.light_intensity, 0.0f, 8.0f, "%.2f");
            ImGui::SliderFloat("Sun azimuth", &debug.light_azimuth, -180.0f, 180.0f, "%.0f deg");
            ImGui::SliderFloat("Sun elevation", &debug.light_elevation, 5.0f, 89.0f, "%.0f deg");
            ImGui::SliderFloat("Bloom", &debug.bloom, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Shadows", &debug.shadow_strength, 0.0f, 1.0f, "%.2f");
            if (debug.net_role && debug.net_role[0]) {
                ImGui::Text("Net  %s  |  tick %u  |  %u peers  |  %.0f ms", debug.net_role, debug.net_tick,
                            debug.net_peers, debug.net_ping_ms);
                ImGui::Text("Streamed  %u entities", debug.net_streamed);
                if (debug.gamemode && debug.gamemode[0])
                    ImGui::Text("Gamemode  %s  |  %u players on server", debug.gamemode, debug.script_players);
                ImGui::SliderFloat("Stream radius", &debug.stream_radius, 16.0f, 160.0f, "%.0f m");
            } else if (debug.stream_radius > 0.0f) {
                ImGui::SliderFloat("Stream radius", &debug.stream_radius, 32.0f, 192.0f, "%.0f m");
                ImGui::Checkbox("Frustum cull", &debug.frustum_cull);
                ImGui::Text("Stream  %u sectors  |  %u in", debug.sectors_loaded, debug.entities_streamed);
                ImGui::Text("Instances  %u drawn  /  %u culled", debug.instances_drawn, debug.instances_culled);
            } else if (debug.gamemode && debug.gamemode[0]) {
                ImGui::Text("Gamemode  %s", debug.gamemode);
                ImGui::Text("Script  %u players  %u veh  %u markers  %u labels", debug.script_players,
                            debug.script_vehicles, debug.script_markers, debug.script_labels);
                if (debug.has_vehicle)
                    ImGui::Text("%s  |  %.1f m/s", debug.driving ? "Driving" : "On foot", debug.speed);
            } else if (debug.has_vehicle) {
                ImGui::Text("%s  |  %.1f m/s  |  %s", debug.driving ? "Driving" : "On foot", debug.speed,
                            debug.clip && debug.clip[0] ? debug.clip : "-");
            } else {
                ImGui::Checkbox("Walk (F)", &debug.walk_mode);
            }
            if (ImGui::Button("Reset camera")) {
                camera.position = debug.home_position;
                camera.yaw = debug.home_yaw;
                camera.pitch = debug.home_pitch;
            }
            ImGui::Separator();
            if (debug.help && debug.help[0]) ImGui::TextUnformatted(debug.help);
            ImGui::TextUnformatted("Shift: boost | F1: overlay | Esc: release / quit");
        }
        ImGui::End();
    }
    if (debug.world_label_count > 0) {
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
            if (ndc.z < 0.0f || ndc.z > 1.0f || ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f)
                continue;
            const ImVec2 pos{(ndc.x * 0.5f + 0.5f) * display.x, (1.0f - (ndc.y * 0.5f + 0.5f)) * display.y};
            const ImVec2 size = ImGui::CalcTextSize(text);
            const ImVec2 aligned{pos.x - size.x * 0.5f, pos.y - size.y};
            draw->AddText(ImVec2(aligned.x + 1.0f, aligned.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
            draw->AddText(aligned, IM_COL32(240, 240, 245, 255), text);
        }
    }
    ImGui::Render();
}

void Overlay::prepare(SDL_GPUCommandBuffer* command)
{
    if (initialized_) ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), command);
}

void Overlay::render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass)
{
    if (initialized_) ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), command, pass);
}
#else
bool Overlay::init(SDL_Window*, SDL_GPUDevice*) { return true; }
void Overlay::shutdown() {}
void Overlay::process_event(const SDL_Event&) {}
bool Overlay::wants_mouse() const { return false; }
bool Overlay::wants_keyboard() const { return false; }
void Overlay::build(Camera&, forge::render::DebugState&, const char*, Uint32, Uint32, Uint32, bool) {}
void Overlay::prepare(SDL_GPUCommandBuffer*) {}
void Overlay::render(SDL_GPUCommandBuffer*, SDL_GPURenderPass*) {}
#endif

} // namespace forge::dev
