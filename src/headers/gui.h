#pragma once

#include "application.h"

namespace Raekor {
namespace gui {

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

//////////////////////////////////////////////////////////////////////////////////////////////////

void setFont(const std::string& filepath);

//////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 getMousePosWindow(const Viewport& viewport, ImVec2 windowPos);

//////////////////////////////////////////////////////////////////////////////////////////////////

class TopMenuBar {
public:
    void draw(WindowApplication* app, Scene& scene, unsigned int activeTexture, entt::entity& active);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Dockspace {
public:
    void begin();
    void end();
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class MetricsWindow {
public:
    void draw(Viewport& viewport, ImVec2 pos);
    void draw(Viewport& viewport);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class InspectorWindow {
public:
    void draw(entt::registry& scene, entt::entity& entity);

private:
    void drawComponent(ecs::NameComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::NodeComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MeshComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MaterialComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::TransformComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::PointLightComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MeshAnimationComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::DirectionalLightComponent& component, entt::registry& scene, entt::entity& active);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class EntityWindow {
public:
    void draw(entt::registry& scene, entt::entity& active);

private:
    void drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active);
    bool drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active);
    void drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Guizmo {
    friend class ViewportWindow;

public:
    void drawWindow();
    ImGuizmo::OPERATION getOperation();

private:
    bool enabled = true;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
    std::array<const char*, 3> previews = { "TRANSLATE", "ROTATE", "SCALE"};
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class ViewportWindow {
public:
    bool begin(Viewport& viewport, unsigned int texture);
    void end();

    void drawGizmo(const Guizmo& gizmo, entt::registry& scene, Viewport& viewport, entt::entity active);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class AssetWindow {
public:
    void drawWindow(entt::registry& assets, entt::entity& active);
};

class CameraSettings {
public:
    void drawWindow(Camera& camera) {
        ImGui::Begin("Camera Properties");
        if (ImGui::DragFloat("Move Speed", &camera.moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Move Constant", &camera.moveConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Speed", &camera.lookSpeed, 0.1f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Constant", &camera.lookConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Speed", &camera.zoomSpeed, 0.001f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Constant", &camera.zoomConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}

        ImGui::End();
    }
};

} // gui
} // raekor