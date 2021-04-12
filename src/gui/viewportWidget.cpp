#include "pch.h"
#include "viewportWidget.h"
#include "editor.h"

#include "IconsFontAwesome5.h"

namespace Raekor {

ViewportWidget::ViewportWidget(Editor* editor) : 
    IWidget(editor),
    mouseInViewport(false),
    rendertarget(&editor->renderer.tonemappingPass->result)
{}



void ViewportWidget::draw() {
    auto& viewport = editor->getViewport();

    // renderer viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4.0f));
    ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Checkbox("Gizmo", &enabled)) {
        ImGuizmo::Enable(enabled);
    } ImGui::SameLine();

    if (ImGui::RadioButton("Move", operation == ImGuizmo::OPERATION::TRANSLATE)) {
        operation = ImGuizmo::OPERATION::TRANSLATE;
    } ImGui::SameLine();

    if (ImGui::RadioButton("Rotate", operation == ImGuizmo::OPERATION::ROTATE)) {
        operation = ImGuizmo::OPERATION::ROTATE;
    } ImGui::SameLine();

    if (ImGui::RadioButton("Scale", operation == ImGuizmo::OPERATION::SCALE)) {
        operation = ImGuizmo::OPERATION::SCALE;
    }

    ImGui::SameLine((ImGui::GetContentRegionAvail().x / 2) - ImGui::GetFrameHeightWithSpacing() * 1.5f);

    ImGui::Button(ICON_FA_HAMMER);
    ImGui::SameLine();
    ImGui::Button(ICON_FA_PLAY);
    ImGui::SameLine();
    ImGui::Button(ICON_FA_STOP);

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.resize({ size.x, size.y });
        editor->renderer.createResources(viewport);
        resized = true;
    }

    // render the active screen texture to the view port as an imgui image
    ImGui::Image((void*)((intptr_t)*rendertarget), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0, 1 }, { 1, 0 });

    // the viewport image is a drag and drop target for dropping materials onto meshes
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material")) {
            auto mousePos = gui::getMousePosWindow(viewport, ImGui::GetWindowPos());
            uint32_t pixel = editor->renderer.GBufferPass->readEntity(mousePos.x, mousePos.y);
            entt::entity picked = static_cast<entt::entity>(pixel);

            if (editor->scene->valid(picked)) {
                auto mesh = editor->scene->try_get<ecs::MeshComponent>(picked);
                if (mesh) {
                    mesh->material = *reinterpret_cast<const entt::entity*>(payload->Data);
                    editor->active = picked;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    auto pos = ImGui::GetWindowPos();
    viewport.offset = { pos.x, pos.y };

    mouseInViewport = ImGui::IsWindowHovered();

    auto& io = ImGui::GetIO();
    if (io.MouseClicked[0] && mouseInViewport && !(editor->active != entt::null && ImGuizmo::IsOver(operation))) {
        auto mousePos = gui::getMousePosWindow(viewport, ImGui::GetWindowPos());
        uint32_t pixel = editor->renderer.GBufferPass->readEntity(mousePos.x, mousePos.y);
        entt::entity picked = static_cast<entt::entity>(pixel);

        if (editor->scene->valid(picked)) {
            editor->active = editor->active == picked ? entt::null : picked;
        } else {
            editor->active = entt::null;
        }
    }

    if (editor->active != entt::null && enabled) {
        if (!editor->scene->valid(editor->active) || 
            !editor->scene->has<ecs::TransformComponent>(editor->active)) {
        }

        // set the gizmo's viewport
        ImGuizmo::SetDrawlist();
        auto pos = ImGui::GetWindowPos();
        ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);

        // temporarily transform to mesh space for gizmo use
        auto& transform = editor->scene->get<ecs::TransformComponent>(editor->active);
        auto mesh = editor->scene->try_get<ecs::MeshComponent>(editor->active);
        if (mesh) {
            transform.localTransform = glm::translate(transform.localTransform, ((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));
        }

        // draw gizmo
        bool manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(viewport.getCamera().getView()),
            glm::value_ptr(viewport.getCamera().getProjection()),
            operation, ImGuizmo::MODE::LOCAL,
            glm::value_ptr(transform.localTransform)
        );

        // transform back to world space
        if (mesh) {
            transform.localTransform = glm::translate(transform.localTransform, -((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));
        }

        // update the transformation
        if (manipulated) {
            transform.decompose();
        }
    }

    auto metricsPosition = ImGui::GetWindowPos();
    metricsPosition.y += ImGui::GetFrameHeightWithSpacing();
    ImGui::SetNextWindowPos(metricsPosition);

    ImGui::End();
    ImGui::PopStyleVar();

    // application/render metrics
    ImGui::SetNextWindowBgAlpha(0.35f);
    auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
    ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
    ImGui::Text("Product: %s", glGetString(GL_RENDERER));
    ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
    ImGui::End();
}

}