#include "pch.h"
#include "viewportWidget.h"
#include "editor.h"

namespace Raekor {

ViewportWidget::ViewportWidget(Editor* editor) :
    IWidget(editor, "Viewport"),
    rendertarget(IWidget::renderer().tonemap->result)
{}



void ViewportWidget::draw() {

    auto& scene = IWidget::scene();
    auto& renderer = IWidget::renderer();
    auto& viewport = editor->getViewport();

    // renderer viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4.0f));
    ImGui::Begin(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Checkbox("Gizmo", &gizmoEnabled)) {
        ImGuizmo::Enable(gizmoEnabled);
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

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 256.0f);

    constexpr std::array items = {
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

    const std::array targets = {
        renderer.tonemap->result,
        renderer.gbuffer->albedoTexture,
        renderer.gbuffer->normalTexture,
        renderer.gbuffer->materialTexture,
        renderer.gbuffer->velocityTexture,
        renderer.taaResolve->resultBuffer,
        renderer.deferShading->result,
        renderer.deferShading->bloomHighlights,
        renderer.bloom->blurTexture,
        renderer.bloom->bloomTexture,
    };

    for (int i = 0; i < targets.size(); i++) {
        if (rendertarget == targets[i]) {
            rendertargetIndex = i;
        }
    }

    if (ImGui::Combo("##Render target", &rendertargetIndex, items.data(), int(items.size()))) {
        rendertarget = targets[rendertargetIndex];
    }

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.resize({ size.x, size.y });
        renderer.createRenderTargets(viewport);
        resized = true;
    }

    focused = ImGui::IsWindowFocused();

    // render the active screen texture to the view port as an imgui image
    ImGui::Image((void*)((intptr_t)rendertarget), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0, 1 }, { 1, 0 });

    const ImVec2 viewportMin = ImGui::GetItemRectMin();
    const ImVec2 viewportMax = ImGui::GetItemRectMax();

    // the viewport image is a drag and drop target for dropping materials onto meshes
    if (ImGui::BeginDragDropTarget()) {
        auto mousePos = GUI::getMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        uint32_t pixel = renderer.gbuffer->readEntity(mousePos.x, mousePos.y);
        entt::entity picked = static_cast<entt::entity>(pixel);

        Mesh* mesh = nullptr;

        if (scene.valid(picked)) {
            ImGui::BeginTooltip();

            mesh = scene.try_get<Mesh>(picked);

            if (mesh) {
                ImGui::Text(std::string(std::string("Apply to ") + scene.get<Name>(picked).name).c_str());
                editor->active = picked;
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                ImGui::Text("Invalid target");
                ImGui::PopStyleColor();
            }

            ImGui::EndTooltip();
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material")) {
            if (mesh) {
                mesh->material = *reinterpret_cast<const entt::entity*>(payload->Data);
                editor->active = mesh->material;
            }
        }

        ImGui::EndDragDropTarget();
    }

    auto pos = ImGui::GetWindowPos();
    viewport.offset = { pos.x, pos.y };

    mouseInViewport = ImGui::IsMouseHoveringRect(viewportMin, viewportMax);

    auto& io = ImGui::GetIO();
    if (io.MouseClicked[0] && mouseInViewport && !(editor->active != entt::null && ImGuizmo::IsOver(operation)) && !ImGui::IsAnyItemHovered()) {
        auto mousePos = GUI::getMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        uint32_t pixel = renderer.gbuffer->readEntity(mousePos.x, mousePos.y);
        entt::entity picked = static_cast<entt::entity>(pixel);

        if (scene.valid(picked)) {
            editor->active = editor->active == picked ? entt::null : picked;
        } else {
            editor->active = entt::null;
        }
    }


    if (editor->active != entt::null && scene.valid(editor->active) && scene.all_of<Transform>(editor->active) && gizmoEnabled) {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

        // temporarily transform to mesh space for gizmo use
        Transform& transform = scene.get<Transform>(editor->active);
        Mesh* mesh = scene.try_get<Mesh>(editor->active);

        if (mesh) {
            transform.localTransform = glm::translate(transform.localTransform, ((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));
        }

        // prevent the gizmo from going outside of the viewport
        ImGui::GetWindowDrawList()->PushClipRect(viewportMin, viewportMax);

        bool manipulated = ImGuizmo::Manipulate(
                               glm::value_ptr(viewport.getCamera().getView()),
                               glm::value_ptr(viewport.getCamera().getProjection()),
                               operation, ImGuizmo::MODE::WORLD,
                               glm::value_ptr(transform.localTransform)
                           );

        // transform back to world space
        if (mesh) {
            transform.localTransform = glm::translate(transform.localTransform, -((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));
        }

        if (manipulated) {
            transform.decompose();
        }
    }

    ImVec2 metricsPosition = ImGui::GetWindowPos();
    metricsPosition.y += ImGui::GetFrameHeightWithSpacing();
    ImGui::SetNextWindowPos(metricsPosition);

    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGuiWindowFlags metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
    ImGui::Text("Culled meshes: %i", renderer.gbuffer->culled);
    ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
    ImGui::Text("Product: %s", glGetString(GL_RENDERER));
    ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
    ImGui::End();
}



void ViewportWidget::onEvent(const SDL_Event& ev) {
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