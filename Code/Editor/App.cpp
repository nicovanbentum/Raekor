#include "pch.h"
#include "App.h"

#include <locale>
#include <codecvt>

#include "Engine/OS.h"
#include "Engine/Iter.h"
#include "Engine/Timer.h"
#include "Engine/Input.h"
#include "Engine/Archive.h"
#include "Engine/Profiler.h"
#include "Engine/DebugRenderer.h"

#include "Engine/Renderer/Shared.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderUtil.h"
#include "Engine/Renderer/RayTracing.h"
#include "Engine/Renderer/RenderGraph.h"
#include "Engine/Renderer/GPUProfiler.h"

extern float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(int pixel_i, int pixel_j, int sampleIndex, int sampleDimension);

namespace RK::DX12 {

RTTI_DEFINE_TYPE_NO_FACTORY(GPUProfileWidget) {}
RTTI_DEFINE_TYPE_NO_FACTORY(DeviceResourcesWidget) {}

DXApp::DXApp() :
    Editor(WindowFlag::RESIZE, &m_RenderInterface),
    m_Device(this),
    m_Renderer(m_Device, m_Viewport, m_Window),
    m_RayTracedScene(m_Scene),
    m_RenderInterface(this, m_Device, m_Renderer)
{
    g_GPUProfiler = new GPUProfiler(m_Device);

    g_RTTIFactory.Register(RTTI_OF<ComputeProgram>());
    g_RTTIFactory.Register(RTTI_OF<GraphicsProgram>());
    g_RTTIFactory.Register(RTTI_OF<SystemShadersDX12>());

    Timer timer;

    JSON::ReadArchive read_archive("Assets\\Shaders\\Backend\\Shaders.json");
    read_archive >> g_SystemShaders;

    // compile shaders
    g_SystemShaders.OnCompile(m_Device);
    g_ThreadPool.WaitForJobs();

    if (!g_SystemShaders.IsCompiled())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DX12 Error", "Failed to compile system shaders", m_Window);
        std::abort();
    }

    LogMessage(std::format("[CPU] Shader compilation took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Creating the SRVs at heap index 0 results in a 4x4 black square in the top left of the texture,
    // this is a hacky workaround. at least we get the added benefit of 0 being an 'invalid' index :D
    (void)m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Add(nullptr);

    static Array<Vec4> blue_noise_samples;
    blue_noise_samples.reserve(128 * 128);

    for (int y = 0; y < 128; y++)
    {
        for (int x = 0; x < 128; x++)
        {
            Vec4& sample = blue_noise_samples.emplace_back();

            for (int i = 0; i < sample.length(); i++)
                sample[i] = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(x, y, 0, i);
        }
    }

    TextureID bluenoise_texture = m_Device.CreateTexture(Texture::Desc
    {
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .width  = 128,
        .height = 128,
        .usage  = Texture::Usage::SHADER_READ_ONLY,
        .debugName = "BlueNoise128x1spp"
    });

    assert(m_Device.GetBindlessHeapIndex(bluenoise_texture) == BINDLESS_BLUE_NOISE_TEXTURE_INDEX);

    m_Device.UploadTextureData(m_Device.GetTexture(bluenoise_texture), 0, 0, sizeof(Vec4) * 128, blue_noise_samples.data());

    LogMessage(std::format("[CPU] Blue noise texture took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // Create default textures / assets
    const String black_texture_file = "Assets/black4x4.dds";
    const String white_texture_file = "Assets/white4x4.dds";
    const String normal_texture_file = "Assets/normal4x4.dds";
    m_DefaultBlackTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(black_texture_file)));
    m_DefaultWhiteTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(white_texture_file)));
    m_DefaultNormalTexture = TextureID(m_RenderInterface.UploadTextureFromAsset(m_Assets.GetAsset<TextureAsset>(normal_texture_file)));

    assert(m_DefaultBlackTexture.IsValid() && m_DefaultBlackTexture.GetIndex() != 0);
    assert(m_DefaultWhiteTexture.IsValid() && m_DefaultWhiteTexture.GetIndex() != 0);
    assert(m_DefaultNormalTexture.IsValid() && m_DefaultNormalTexture.GetIndex() != 0);

    Material::Default.gpuAlbedoMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuNormalMap = m_DefaultNormalTexture.GetValue();
    Material::Default.gpuEmissiveMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuMetallicMap = m_DefaultWhiteTexture.GetValue();
    Material::Default.gpuRoughnessMap = m_DefaultWhiteTexture.GetValue();

    m_RenderInterface.SetBlackTexture(m_DefaultBlackTexture.GetValue());
    m_RenderInterface.SetWhiteTexture(m_DefaultWhiteTexture.GetValue());

    LogMessage(std::format("[CPU] Default material upload took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // initialize ImGui
    ImGui_ImplSDL3_InitForD3D(m_Window);
    m_ImGuiFontTextureID = InitImGui(m_Device, Renderer::sSwapchainFormat, sFrameCount);

    LogMessage(std::format("[CPU] ImGui init took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    // initialize DirectStorage 1.0
    DSTORAGE_QUEUE_DESC queue_desc =
    {
        .SourceType = DSTORAGE_REQUEST_SOURCE_FILE,
        .Capacity = DSTORAGE_MAX_QUEUE_CAPACITY,
        .Priority = DSTORAGE_PRIORITY_NORMAL,
        .Device = *m_Device,
    };

    LogMessage(std::format("[CPU] DirectStorage init took {:.2f} ms", Timer::sToMilliseconds(timer.Restart())));

    m_Widgets.Register<GPUProfileWidget>(this);
    m_Widgets.Register<DeviceResourcesWidget>(this);
    
    //m_Widgets.GetWidget<GPUProfileWidget>()->Hide();
    m_Widgets.GetWidget<DeviceResourcesWidget>()->Hide();

    m_Renderer.Recompile(m_Device, m_RayTracedScene, GetRenderInterface());

    if (!m_ConfigSettings.mSceneFile.empty() && fs::exists(m_ConfigSettings.mSceneFile))
    {
        m_Scene.OpenFromFile(m_ConfigSettings.mSceneFile.string(), m_Assets, this);
    }
}


DXApp::~DXApp()
{
    m_Renderer.WaitForIdle(m_Device);
    m_Device.ReleaseTextureImmediate(m_ImGuiFontTextureID);
}


void DXApp::OnUpdate(float inDeltaTime)
{
    Editor::OnUpdate(inDeltaTime);

    if (m_ViewportChanged || m_GameState == GAME_RUNNING)
        RenderSettings::mPathTraceReset = true;

    for (const Animation& animation : m_Scene.GetComponents<Animation>())
    {
        if (animation.IsPlaying())
        {
            RenderSettings::mPathTraceReset = true;
            break;
        }
    }

    m_RenderInterface.UpdateGPUStats(m_Device);

    m_Renderer.OnRender(this, m_Device, m_Viewport, m_RayTracedScene, GetRenderInterface(), inDeltaTime);
    
    m_Device.OnUpdate();

    g_GPUProfiler->Reset(m_Device);
}


void DXApp::OnEvent(const SDL_Event& inEvent)
{
    Editor::OnEvent(inEvent);

    if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat)
    {
        // ALT + ENTER event (Windowed <-> Fullscreen toggle)
        if (inEvent.key.key == SDLK_RETURN && SDL_GetModState() & SDL_KMOD_LALT)
        {
            // This only toggles between windowed and borderless fullscreen, exclusive fullscreen needs to be set from the menu
            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN)
                SDL_SetWindowFullscreen(m_Window, false);
            else
                SDL_SetWindowFullscreen(m_Window, true);

            // SDL2 should have updated the window by now, so get the new size
            int width = 0, height = 0;
            SDL_GetWindowSize(m_Window, &width, &height);

            // Updat the viewport and tell the renderer to resize to the viewport
            m_Viewport.SetRenderSize(UVec2(width, height));
            m_Renderer.SetShouldResize(true);

            const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode(m_Window);
            LogMessage(std::format("SDL Display Mode: {}x{}@{}Hz \n", mode->w, mode->h, mode->refresh_rate));
        }

        switch (inEvent.key.key)
        {
            case SDLK_Z: // undo
            {
                if (g_Input->IsKeyDown(Key::LCTRL) && GetUndo()->HasUndo())
                    RenderSettings::mPathTraceReset = true;
            } break;

            case SDLK_Y: // redo
            {
                if (g_Input->IsKeyDown(Key::LCTRL) && GetUndo()->HasRedo())
                    RenderSettings::mPathTraceReset = true;
            } break;
        }
    }

    if (inEvent.type == SDL_EVENT_WINDOW_RESIZED)
        m_Renderer.SetShouldResize(true);
}


void DeviceResourcesWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();

    DXApp* app = (DXApp*)m_Editor;

    ImGui::Text("UploadBuffers: %.1f MBs", float(app->GetDevice().GetUploadBufferSize()) / 1'000'000);
    
    const RenderGraph& rg = app->GetRenderer().GetRenderGraph();
    uint64_t rg_resources_size = rg.GetResources().GetAllocator().GetSize();

    ImGui::Text("RenderGraphResourceAllocator: %.1f MBs", float(rg_resources_size) / 1'000'000);
    
    const Buffer::Pool& buffer_pool = app->GetDevice().GetBufferPool();
    const Texture::Pool& texture_pool = app->GetDevice().GetTexturePool();

    constexpr static std::array buffer_usage_names =
    {
        "General", 
        "Upload",
        "Readback",
        "IndexBuffer",
        "VertexBuffer",
        "ShaderReadOnly",
        "ShaderReadWrite",
        "IndirectArguments",
        "AccelerationStructure"
    };

    constexpr static std::array texture_usage_names =
    {
        "General",
        "ShaderReadOnly",
        "ShaderReadWrite",
        "RenderTarget",
        "DepthStencilTarget"
    };

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.15, 0.15, 0.15, 1.0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.20, 0.20, 0.20, 1.0));

    uint64_t buffers_total_size = 0;
    uint64_t textures_total_size = 0;

    m_SeenResources.clear();

    if (ImGui::CollapsingHeader("Buffers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Usage Filter And Padding").x);

        if (ImGui::BeginCombo("##BufferUsageFilter", "Usage Filter"))
        {
            for (const auto& [index, usage_name] : gEnumerate(buffer_usage_names))
            {
                bool enabled = m_BufferUsageFilter & ( 1 << index );
                
                if (ImGui::Checkbox(usage_name, &enabled))
                    m_BufferUsageFilter ^= ( 1 << index );
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        ImGui::Text("Total Size: %.1f MBs", float(m_BuffersTotalSize) / 1'000'000);
        
        if (ImGui::BeginTable("Buffers", 3, table_flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Usage");

            for (const auto& [index, buffer] : gEnumerate(buffer_pool))
            {
                const ID3D12Resource* resource = buffer.GetD3D12Resource();
                const D3D12MA::Allocation* allocation = buffer.GetD3D12Allocation();
    
                if (resource == nullptr)
                    continue;

                if ((m_BufferUsageFilter & ( 1 << buffer.GetUsage())) == 0)
                    continue;

                if (m_SeenResources.contains(resource))
                    continue;
                else
                    m_SeenResources.insert(resource);

                int byte_size = buffer.GetDesc().size;

                if (allocation)
                    byte_size = allocation->GetSize();

                buffers_total_size += byte_size;

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                if (buffer.GetDesc().debugName)
                    ImGui::Text(buffer.GetDesc().debugName);
                else
                    ImGui::Text("N/A");

                ImGui::TableNextColumn();

                ImGui::Text("%.2f Kib", byte_size / 1000.0f);

                ImGui::TableNextColumn();

                ImGui::Text(buffer_usage_names[buffer.GetUsage()]);
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Usage Filter And Padding").x);

        if (ImGui::BeginCombo("##TextureUsageFilter", "Usage Filter"))
        {
            for (const auto& [index, usage_name] : gEnumerate(texture_usage_names))
            {
                bool enabled = m_TextureUsageFilter & ( 1 << index );

                if (ImGui::Checkbox(usage_name, &enabled))
                    m_TextureUsageFilter ^= ( 1 << index );
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        ImGui::Text("Total Size: %.1f MBs", float(m_TexturesTotalSize) / 1'000'000);

        if (ImGui::BeginTable("Textures", 3, table_flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Usage");

            for (const auto& [index, texture] : gEnumerate(texture_pool))
            {
                const ID3D12Resource* resource = texture.GetD3D12Resource();
                const D3D12MA::Allocation* allocation = texture.GetD3D12Allocation();

                if (resource == nullptr)
                    continue;
                
                if ((m_TextureUsageFilter & ( 1 << texture.GetUsage())) == 0)
                    continue;
                
                if (m_SeenResources.contains(resource))
                    continue;
                else
                    m_SeenResources.insert(resource);

                int byte_size = gBitsPerPixel(texture.GetFormat()) * ( texture.GetWidth() * texture.GetHeight() ) / 8.0f;

                if (allocation)
                    byte_size = allocation->GetSize();

                textures_total_size += byte_size;
                
                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                if (texture.GetDesc().debugName)
                    ImGui::Text(texture.GetDesc().debugName);
                else
                    ImGui::Text("N/A");

                ImGui::TableNextColumn();

                ImGui::Text("%.2f Mib", byte_size / 1'000'000.0f);

                ImGui::TableNextColumn();

                ImGui::Text(texture_usage_names[texture.GetUsage()]);
            }

            ImGui::EndTable();
        }
    }

    m_BuffersTotalSize = buffers_total_size;
    m_TexturesTotalSize = textures_total_size;

    ImGui::PopStyleColor(2);
    ImGui::End();
}

void GPUProfileWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
    DXApp* app = (DXApp*)m_Editor;

    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter:");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::CalcTextSize("RK::DX12::RenderGraph::Execute").x);
    ImGui::InputText("##profilerfilterinput", &m_FilterInputBuffer);
    ImGui::SameLine();

    ImGuiTextFilter filter = ImGuiTextFilter(m_FilterInputBuffer.c_str());

    bool enabled = g_GPUProfiler->IsEnabled();
    if (ImGui::Checkbox(enabled ? "Running" : "Paused", &enabled))
        g_GPUProfiler->SetEnabled(enabled);

    ImGui::SameLine();

    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("Zoom##profilerzoom", &m_Zoom, 1.0f, 10.0f, "%.1f");

    uint64_t frequency = 0;
    gThrowIfFailed(app->GetDevice().GetGraphicsQueue()->GetTimestampFrequency(&frequency));

    uint32_t max_depth = 0;
    uint64_t lowest_tick = UINT64_MAX;
    uint64_t highest_tick = 0;

    for (const GPUProfileSection& section : g_GPUProfiler->GetGPUProfileSections())
    {
        max_depth = glm::max(max_depth, section.mDepth);
        lowest_tick = glm::min(lowest_tick, section.mStartTick);
        highest_tick = glm::max(highest_tick, section.mEndTick);
    }

    float total_time = ( highest_tick - lowest_tick ) / (double)frequency;

    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    ImVec2 avail_size = ImGui::GetContentRegionAvail();

    float bar_height = ( avail_size.y / ( max_depth + 1 ) ) * m_Zoom;
    float pixels_per_tick = avail_size.x / ( highest_tick - lowest_tick );

    for (const auto& [index, section] : gEnumerate(g_GPUProfiler->GetGPUProfileSections()))
    {
        // skip sections that took (almost) no GPU time
        if (section.mEndTick - section.mStartTick < 50)
            continue;

        const float end_pos_x = start_pos.x + ( section.mEndTick - lowest_tick ) * pixels_per_tick;
        const float start_pos_x = start_pos.x + ( section.mStartTick - lowest_tick ) * pixels_per_tick;

        const float end_pos_y = start_pos.y + ( section.mDepth + 1 ) * bar_height;
        const float start_pos_y = start_pos.y + ( section.mDepth + 0 ) * bar_height;

        const ImVec2 pad = ImVec2(1.0f, 1.0f);
        const ImRect bbox = ImRect(ImVec2(start_pos_x, start_pos_y), ImVec2(end_pos_x, end_pos_y));

        const float time = ( section.mEndTick - section.mStartTick ) / (double)frequency;
        const float time_pct = time / total_time;

        if (time_pct < 0.0001f)
            continue;

        char text_buffer[255];
        ImFormatString(text_buffer, std::size(text_buffer), "%s (%.2f ms)", section.mName, time * 1'000);

        ImGui::PushID(index);

        ImGui::SetCursorScreenPos(bbox.Min);

        ImGui::InvisibleButton(section.mName, bbox.GetSize(), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        ImGui::PopID();

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            m_SelectedSectionIndex = m_SelectedSectionIndex == index ? -1 : index;
        }

        //ImGui::DebugDrawItemRect();

        bool is_hovered = ImGui::IsItemHovered();

        if (is_hovered)
            ImGui::SetTooltip(text_buffer);

        float label_hash = float(gHash32Bit(section.mName)) / UINT32_MAX;

        float v = 0.5f;
        float upper_s = is_hovered ? 0.6f : 0.5f;
        float lower_s = is_hovered ? 0.5f : 0.4f;

        const bool passes_filter = filter.PassFilter(section.mName);

        if (!passes_filter)
        {
            float v = 0.2f;
            upper_s = 0.2f;
            lower_s = 0.2f;
        }

        ImVec4 upper_hsv = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImVec4 lower_hsv = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImGui::ColorConvertHSVtoRGB(label_hash, 0.5f, upper_s, upper_hsv.x, upper_hsv.y, upper_hsv.z);
        ImGui::ColorConvertHSVtoRGB(label_hash, 0.5f, lower_s, lower_hsv.x, lower_hsv.y, lower_hsv.z);

        ImU32 upper_gradient = ImGui::ColorConvertFloat4ToU32(upper_hsv);
        ImU32 lower_gradient = ImGui::ColorConvertFloat4ToU32(lower_hsv);
            
        ImGui::GetWindowDrawList()->AddRectFilled(bbox.Min, bbox.Max, m_SelectedSectionIndex == index ? IM_COL32_WHITE : IM_COL32_BLACK);
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(bbox.Min + pad, bbox.Max - pad, upper_gradient, lower_gradient, lower_gradient, upper_gradient);
            
        // dont render labels for tiny bars
        if (time_pct > 0.01f)
        {
            const ImVec2 label_size = ImGui::CalcTextSize(text_buffer, NULL, true);
            const ImRect text_clip_rect = ImRect(bbox.Min + pad, bbox.Max - pad * 2.0f);
            const ImColor text_color = passes_filter ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(text_color));
            
            ImGui::RenderTextClipped(bbox.Min + pad * 2.0f, bbox.Max - pad * 2.0f, text_buffer, NULL, &label_size, ImGui::GetStyle().ButtonTextAlign, &text_clip_rect);
            
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}


void GPUProfileWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{
    if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat)
    {
        switch (inEvent.key.key)
        {
            case SDLK_SPACE:
            {
                g_GPUProfiler->SetEnabled(!g_GPUProfiler->IsEnabled());
            } break;
        }
    }
}

} // namespace::Raekor