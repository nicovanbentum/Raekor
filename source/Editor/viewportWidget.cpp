#include "pch.h"
#include "viewportWidget.h"
#include "Raekor/scene.h"
#include "Raekor/gui.h"
#include "Raekor/application.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Application* inApp) :
    IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_VIDEO " Viewport ")),
    m_DisplayTexture(inApp->GetRenderer()->GetDisplayTexture())
{}


void ViewportWidget::Draw(float dt) {
    auto& scene = IWidget::GetScene();
    auto& physics  = IWidget::GetPhysics();
    auto& viewport = m_Editor->GetViewport();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4.0f));
    const auto flags =  ImGuiWindowFlags_AlwaysAutoResize | 
                        ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_NoScrollbar;

    ImGui::SetNextWindowSize(ImVec2(160, 90), ImGuiCond_FirstUseEver);
    m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open, flags);
    m_Editor->GetRenderer()->GetSettings().paused = !m_Visible;

    if (ImGui::Checkbox("Gizmo", &gizmoEnabled))
        ImGuizmo::Enable(gizmoEnabled);
    ImGui::SameLine();

    if (ImGui::RadioButton("Move", operation == ImGuizmo::OPERATION::TRANSLATE))
        operation = ImGuizmo::OPERATION::TRANSLATE;
    ImGui::SameLine();

    if (ImGui::RadioButton("Rotate", operation == ImGuizmo::OPERATION::ROTATE))
        operation = ImGuizmo::OPERATION::ROTATE;
    ImGui::SameLine();

    if (ImGui::RadioButton("Scale", operation == ImGuizmo::OPERATION::SCALE))
        operation = ImGuizmo::OPERATION::SCALE;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 256.0f);

    static constexpr auto items = std::array {
        "Final",
        "Albedo",
        "Normals",
        "Material",
        "Velocity",
        "TAA Resolve",
        "Shading (Result)",
        "Bloom (Threshold)",
        "Bloom (Blur 1)",
        "Bloom (Final)",
    };

    //const auto targets = std::array {
    //    renderer.m_Tonemap->result,
    //    renderer.m_GBuffer->albedoTexture,
    //    renderer.m_GBuffer->normalTexture,
    //    renderer.m_GBuffer->materialTexture,
    //    renderer.m_GBuffer->velocityTexture,
    //    renderer.m_ResolveTAA->resultBuffer,
    //    renderer.m_DeferredShading->result,
    //    renderer.m_DeferredShading->bloomHighlights,
    //    renderer.m_Bloom->blurTexture,
    //    renderer.m_Bloom->bloomTexture,
    //};

    //for (int i = 0; i < targets.size(); i++)
    //    if (rendertarget == targets[i])
    //        rendertargetIndex = i;

    if (ImGui::Combo("##Render target", &rendertargetIndex, items.data(), int(items.size()))) {
    
    }

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    size.x = glm::max(size.x, 160.0f);
    size.y = glm::max(size.y, 90.0f);

    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.SetSize({ size.x, size.y });
        m_Editor->GetRenderer()->OnResize(viewport);
        m_Editor->LogMessage("Render Size: " + glm::to_string(viewport.GetSize()));
        resized = true;
    }

    // update focused state
    m_Focused = ImGui::IsWindowFocused();

    // Update the display texture incase it changed
    m_DisplayTexture = m_Editor->GetRenderer()->GetDisplayTexture();

    // calculate display UVs
    auto uv0 = ImVec2(0, 0);
    auto uv1 = ImVec2(1, 1);

    // Flip the Y uv for OpenGL and Vulkan
    if ((m_Editor->GetRenderer()->GetGraphicsAPI() & GraphicsAPI::DirectX) == 0)
       std::swap(uv0.y, uv1.y);

    // Render the display image
    const auto image_size = ImVec2(size.x, size.y);
    const auto border_color = GetPhysics().GetState() != Physics::Idle ? GetPhysics().GetStateColor() : ImVec4(0, 0, 0, 1);
    ImGui::Image((void*)((intptr_t)m_DisplayTexture), size, uv0, uv1, ImVec4(1, 1, 1, 1), border_color);

    mouseInViewport = ImGui::IsItemHovered();
    const ImVec2 viewportMin = ImGui::GetItemRectMin();
    const ImVec2 viewportMax = ImGui::GetItemRectMax();

    // the viewport image is a drag and drop target for dropping materials onto meshes
    if (ImGui::BeginDragDropTarget()) {
        const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        const auto pixel = m_Editor->GetRenderer()->GetSelectedEntity(mouse_pos.x, mouse_pos.y);
        const auto picked = Entity(pixel);

        Mesh* mesh = nullptr;

        if (scene.valid(picked)) {
            ImGui::BeginTooltip();

            mesh = scene.try_get<Mesh>(picked);

            if (mesh) {
                ImGui::Text(std::string(std::string("Apply to ") + scene.get<Name>(picked).name).c_str());
                SetActiveEntity(picked);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                ImGui::Text("Invalid target");
                ImGui::PopStyleColor();
            }

            ImGui::EndTooltip();
        }

        const auto payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material");

        if (payload && mesh) {
            mesh->material = *reinterpret_cast<const Entity*>(payload->Data);
            SetActiveEntity(mesh->material);
        }

        ImGui::EndDragDropTarget();
    }

    auto pos = ImGui::GetWindowPos();
    viewport.offset = { pos.x, pos.y };

    if (ImGui::GetIO().MouseClicked[0] && mouseInViewport && !(GetActiveEntity() != sInvalidEntity && ImGuizmo::IsOver(operation)) && !ImGui::IsAnyItemHovered()) {
        const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        const auto pixel = m_Editor->GetRenderer()->GetSelectedEntity(mouse_pos.x, mouse_pos.y);
        auto picked = Entity(pixel);

        if (scene.valid(picked))
            SetActiveEntity(GetActiveEntity() == picked ? sInvalidEntity : picked);
        else
            SetActiveEntity(sInvalidEntity);
    }

    if (GetActiveEntity() != entt::null && scene.valid(GetActiveEntity()) && scene.all_of<Transform>(GetActiveEntity()) && gizmoEnabled) {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

        // temporarily transform to mesh space for gizmo use
        auto& transform = scene.get<Transform>(GetActiveEntity());
        const auto mesh = scene.try_get<Mesh>(GetActiveEntity());

        if (mesh)
            transform.localTransform = glm::translate(transform.localTransform, ((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));

        // prevent the gizmo from going outside of the viewport
        ImGui::GetWindowDrawList()->PushClipRect(viewportMin, viewportMax);

        const auto manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(viewport.GetCamera().GetView()),
            glm::value_ptr(viewport.GetCamera().GetProjection()),
            operation, ImGuizmo::MODE::LOCAL,
            glm::value_ptr(transform.localTransform)
        );

        // transform back to world space
        if (mesh)
            transform.localTransform = glm::translate(transform.localTransform, -((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));

        if (manipulated)
            transform.Decompose();
    }

    auto metricsPosition = ImGui::GetWindowPos();
    metricsPosition.y += ImGui::GetFrameHeightWithSpacing();

    if (auto dock_node = ImGui::GetCurrentWindow()->DockNode)
        if (!dock_node->IsHiddenTabBar())
            metricsPosition.y += 25.0f;

    ImGui::End();
    ImGui::PopStyleVar();

    if (m_Visible) {
        ImGui::SetNextWindowPos(metricsPosition);
        ImGui::SetNextWindowBgAlpha(0.35f);

        const auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
        // ImGui::Text("Culled meshes: %i", renderer.m_GBuffer->culled);

        const auto& gpu_info = m_Editor->GetRenderer()->GetGPUInfo();

        ImGui::Text("Vendor: %s", gpu_info.mVendor.c_str());
        ImGui::Text("Product: %s", gpu_info.mProduct.c_str());

        ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
        ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::Text("Graphics API: %s", gpu_info.mActiveAPI.c_str());
        ImGui::End();
    }

}


void ViewportWidget::OnEvent(const SDL_Event& ev) {
    if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        switch (ev.key.keysym.sym) {
            case SDLK_r: {
                operation = ImGuizmo::OPERATION::ROTATE;
            } break;
            case SDLK_t: {
                operation = ImGuizmo::OPERATION::TRANSLATE;
            } break;
            case SDLK_s: {
                operation = ImGuizmo::OPERATION::SCALE;
            } break;
        }
    }
}



}