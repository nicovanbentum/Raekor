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

namespace RK::DX12 {

RTTI_DEFINE_TYPE_NO_FACTORY(DeviceResourcesWidget) {}

DXApp::DXApp() :
    Editor(WindowFlag::RESIZE, &m_RenderSystem),
    m_RenderSystem(this)
{
    m_RenderSystem.InitImGui(m_Window);

    m_Widgets.Register<DeviceResourcesWidget>(this);
    
    m_Widgets.GetWidget<DeviceResourcesWidget>()->Hide();

    const String scene_override = OS::sGetCommandLineValue("-scene");
    const Path scene_file = scene_override.empty() ? m_ConfigSettings.mSceneFile : Path(scene_override);

    if (!scene_file.empty() && fs::exists(scene_file))
        OpenScene(scene_file);
}


DXApp::~DXApp()
{
}


void DXApp::OnUpdate(float inDeltaTime)
{
    Editor::OnUpdate(inDeltaTime);

    if (m_ViewportChanged)
        RenderSettings::mPathTraceReset = true;

    m_ViewportChanged = false;

    m_RenderSystem.OnRender(this, m_Scene, inDeltaTime);
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
            m_RenderSystem.GetRenderer().SetShouldResize(true);

            const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode(m_Window);
            gLogInfo("App", "SDL display mode: {}x{}@{}Hz", mode->w, mode->h, mode->refresh_rate);
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
        m_RenderSystem.GetRenderer().SetShouldResize(true);
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

} // namespace::Raekor