#include "pch.h"
#include "viewportWidget.h"
#include "editor.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Editor* editor) :
    IWidget(editor, "Viewport"),
    rendertarget(IWidget::GetRenderer().m_Tonemap->result)
{}


void ViewportWidget::draw(float dt) {
    auto& scene = IWidget::GetScene();
    auto& renderer = IWidget::GetRenderer();
    auto& viewport = editor->GetViewport();
    auto& physics = IWidget::GetPhysics();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4.0f));
    const auto flags =  ImGuiWindowFlags_AlwaysAutoResize | 
                        ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_NoScrollbar;

    const bool is_visible = ImGui::Begin(title.c_str(), NULL, flags);
    renderer.settings.paused = !is_visible;

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

    ImGui::SameLine((ImGui::GetContentRegionAvail().x / 2) - ImGui::GetFrameHeightWithSpacing() * 1.5f);

    if (ImGui::Button(ICON_FA_HAMMER)) {}

    ImGui::SameLine();

    std::array physics_state_text_colors = {
        ImGui::GetStyleColorVec4(ImGuiCol_Text),
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
        ImVec4(0.35f, 0.78f, 1.0f, 1.0f),
    };

    const auto physics_state = physics.GetState();
    ImGui::PushStyleColor(ImGuiCol_Text, physics_state_text_colors[physics_state]);
    if (ImGui::Button(physics_state == Physics::Stepping ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
        switch (physics_state) {
            case Physics::Idle: {
                physics.SaveState();
                physics.SetState(Physics::Stepping);
            } break;
            case Physics::Paused: {
                physics.SetState(Physics::Stepping);
            } break;
            case Physics::Stepping: {
                physics.SetState(Physics::Paused);
            } break;
        }
    }

    ImGui::PopStyleColor();
    ImGui::SameLine();

    const auto current_physics_state = physics_state;
    if (current_physics_state != Physics::Idle)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

    if (ImGui::Button(ICON_FA_STOP)) {
        if (physics_state != Physics::Idle) {
            physics.RestoreState();
            physics.Step(scene, dt); // Step once to trigger the restored state
            physics.SetState(Physics::Idle);
        }
    }

    if (current_physics_state != Physics::Idle)
        ImGui::PopStyleColor();

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
        renderer.m_Tonemap->result,
        renderer.m_GBuffer->albedoTexture,
        renderer.m_GBuffer->normalTexture,
        renderer.m_GBuffer->materialTexture,
        renderer.m_GBuffer->velocityTexture,
        renderer.m_ResolveTAA->resultBuffer,
        renderer.m_DeferredShading->result,
        renderer.m_DeferredShading->bloomHighlights,
        renderer.m_Bloom->blurTexture,
        renderer.m_Bloom->bloomTexture,
    };

    for (int i = 0; i < targets.size(); i++)
        if (rendertarget == targets[i])
            rendertargetIndex = i;

    if (ImGui::Combo("##Render target", &rendertargetIndex, items.data(), int(items.size())))
        rendertarget = targets[rendertargetIndex];

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.Resize({ size.x, size.y });
        renderer.CreateRenderTargets(viewport);
        resized = true;
    }

    focused = ImGui::IsWindowFocused();

    physics_state_text_colors[Physics::Idle] = ImVec4(0.0f, 0.0f, 0.0f, 0.01f);
    std::swap(physics_state_text_colors[Physics::Paused], physics_state_text_colors[Physics::Stepping]);
    const auto border_color = physics_state_text_colors[physics.GetState()];

    const auto image_size = ImVec2(ImVec2((float)viewport.size.x, (float)viewport.size.y));
    ImGui::Image((void*)((intptr_t)rendertarget), image_size, { 0, 1 }, { 1, 0 }, ImVec4(1, 1, 1, 1), border_color);

    mouseInViewport = ImGui::IsItemHovered();
    const ImVec2 viewportMin = ImGui::GetItemRectMin();
    const ImVec2 viewportMax = ImGui::GetItemRectMax();

    auto& active_entity = GetActiveEntity();

    // the viewport image is a drag and drop target for dropping materials onto meshes
    if (ImGui::BeginDragDropTarget()) {
        const auto mousePos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        const auto pixel = renderer.m_GBuffer->readEntity(mousePos.x, mousePos.y);
        const auto picked = Entity(pixel);

        Mesh* mesh = nullptr;

        if (scene.valid(picked)) {
            ImGui::BeginTooltip();

            mesh = scene.try_get<Mesh>(picked);

            if (mesh) {
                ImGui::Text(std::string(std::string("Apply to ") + scene.get<Name>(picked).name).c_str());
                active_entity = picked;
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
            active_entity = mesh->material;
        }

        ImGui::EndDragDropTarget();
    }

    auto pos = ImGui::GetWindowPos();
    viewport.offset = { pos.x, pos.y };

    if (ImGui::GetIO().MouseClicked[0] && mouseInViewport && !(active_entity != sInvalidEntity && ImGuizmo::IsOver(operation)) && !ImGui::IsAnyItemHovered()) {
        const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + (ImGui::GetWindowSize() - size));
        const auto pixel = renderer.m_GBuffer->readEntity(mouse_pos.x, mouse_pos.y);
        const auto picked = Entity(pixel);

        if (scene.valid(picked))
            active_entity = active_entity == picked ? sInvalidEntity : picked;
        else
            active_entity = entt::null;
    }

    if (active_entity != entt::null && scene.valid(active_entity) && scene.all_of<Transform>(active_entity) && gizmoEnabled) {
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

        // temporarily transform to mesh space for gizmo use
        auto& transform = scene.get<Transform>(active_entity);
        const auto mesh = scene.try_get<Mesh>(active_entity);

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

    if (!ImGui::GetCurrentWindow()->DockNode->IsHiddenTabBar())
        metricsPosition.y += 25.0f;

    ImGui::End();
    ImGui::PopStyleVar();

    if (is_visible) {
        ImGui::SetNextWindowPos(metricsPosition);
        ImGui::SetNextWindowBgAlpha(0.35f);

        const auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
        ImGui::Text("Culled meshes: %i", renderer.m_GBuffer->culled);

        ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
        ImGui::Text("Product: %s", glGetString(GL_RENDERER));

        ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
        ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
        ImGui::End();
    }

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