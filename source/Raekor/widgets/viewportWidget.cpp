#include "pch.h"
#include "viewportWidget.h"
#include "scene.h"
#include "gui.h"
#include "physics.h"
#include "application.h"
#include "components.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Application* inApp) :
    IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_VIDEO " Viewport "))
{}


void ViewportWidget::Draw(Widgets* inWidgets, float dt) {
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

    auto& current_debug_texture = m_Editor->GetRenderer()->GetSettings().mDebugTexture;
    const auto debug_texture_count = m_Editor->GetRenderer()->GetDebugTextureCount();
    const auto preview = std::string("Render Output: " + std::string(m_Editor->GetRenderer()->GetDebugTextureName(current_debug_texture))); // allocs, BLEH TODO: FIXME
    
    if (ImGui::BeginCombo("##RenderTarget", preview.c_str())) {
        for (auto texture_idx = 0u; texture_idx < debug_texture_count; texture_idx++) {
            if (ImGui::Selectable(m_Editor->GetRenderer()->GetDebugTextureName(texture_idx), current_debug_texture == texture_idx)) {
                current_debug_texture = texture_idx;
                m_Editor->GetRenderer()->OnResize(viewport); // not an actual resize, just to recreate render targets
            }
        }

        ImGui::EndCombo();
    }

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    size.x = glm::max(size.x, 160.0f);
    size.y = glm::max(size.y, 90.0f);

    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.SetSize({ size.x, size.y });
        m_Editor->GetRenderer()->OnResize(viewport);
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

        if (scene.IsValid(picked)) {
            ImGui::BeginTooltip();

            if (scene.Has<Mesh>(picked)) {
                ImGui::Text(std::string(std::string("Apply to ") + scene.Get<Name>(picked).name).c_str());
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

    if (ImGui::GetIO().MouseClicked[0] && mouseInViewport && !(GetActiveEntity() != NULL_ENTITY && ImGuizmo::IsOver(operation)) && !ImGui::IsAnyItemHovered()) {
        const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        const auto pixel = m_Editor->GetRenderer()->GetSelectedEntity(mouse_pos.x, mouse_pos.y);
        auto picked = Entity(pixel);

        if (GetActiveEntity() == picked) {
            if (auto soft_body = scene.GetPtr<SoftBody>(GetActiveEntity())) {
                if (auto body = GetPhysics().GetSystem()->GetBodyLockInterface().TryGetBody(soft_body->mBodyID)) {
                    const auto ray = Ray(viewport, mouse_pos);
                    const auto& mesh = scene.Get<Mesh>(GetActiveEntity());
                    const auto& transform = scene.Get<Transform>(GetActiveEntity());

                    auto hit_dist = FLT_MAX;
                    auto triangle_index = -1;

                    for (auto i = 0u; i < mesh.indices.size(); i += 3) {
                        const auto v0 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i]], 1.0));
                        const auto v1 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
                        const auto v2 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

                        const auto hit_result = ray.HitsTriangle(v0, v1, v2);

                        if (hit_result.has_value() && hit_result.value() < hit_dist) {
                            hit_dist = hit_result.value();
                            triangle_index = i;
                        }
                    }
                    
                    if (triangle_index > -1) {
                        auto props = (JPH::SoftBodyMotionProperties*)body->GetMotionProperties();
                        props->GetVertex(mesh.indices[triangle_index]).mInvMass = 0.0f;
                        props->GetVertex(mesh.indices[triangle_index + 1]).mInvMass = 0.0f;
                        props->GetVertex(mesh.indices[triangle_index + 2]).mInvMass = 0.0f;
                        
                        m_Editor->LogMessage(std::format("Soft Body triangle hit: {}", triangle_index));
                    }
                }
            }
            else
                SetActiveEntity(NULL_ENTITY);
        }
        else
            SetActiveEntity(picked);
    }

    if (GetActiveEntity() != NULL_ENTITY && scene.IsValid(GetActiveEntity()) && scene.Has<Transform>(GetActiveEntity()) && gizmoEnabled) {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

        // temporarily transform to mesh space for gizmo use
        auto& transform = scene.Get<Transform>(GetActiveEntity());
        const auto mesh = scene.GetPtr<Mesh>(GetActiveEntity());

        if (mesh) {
            const auto bounds = BBox3D(mesh->aabb[0], mesh->aabb[1]).Scale(transform.GetScaleWorldSpace());
            transform.localTransform = glm::translate(transform.localTransform, bounds.GetCenter());
        }

        // prevent the gizmo from going outside of the viewport
        ImGui::GetWindowDrawList()->PushClipRect(viewportMin, viewportMax);

        const auto manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(viewport.GetCamera().GetView()),
            glm::value_ptr(viewport.GetCamera().GetProjection()),
            operation, ImGuizmo::MODE::WORLD,
            glm::value_ptr(transform.localTransform)
        );

        // transform back to world space
        if (mesh) {
            const auto bounds = BBox3D(mesh->aabb[0], mesh->aabb[1]).Scale(transform.GetScaleWorldSpace());
            transform.localTransform = glm::translate(transform.localTransform, -bounds.GetCenter());
        }

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


void ViewportWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev) {
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