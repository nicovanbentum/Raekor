#pragma once

#include "application.h"

namespace Raekor {
namespace gui {

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

//////////////////////////////////////////////////////////////////////////////////////////////////

void setFont(const std::string& filepath);

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
    void draw(Viewport& viewport, ImVec2 pos, ImVec2 size);
    void draw(Viewport& viewport);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class InspectorWindow {
public:
    void draw(entt::registry& scene, entt::entity& entity);

private:
    void drawNameComponent                  (ecs::NameComponent& component);
    void drawNodeComponent                  (ecs::NodeComponent& component);
    void drawMeshComponent                  (ecs::MeshComponent& component, entt::registry& scene, entt::entity& active);
    void drawMaterialComponent              (ecs::MaterialComponent& component);
    void drawTransformComponent             (ecs::TransformComponent& component);
    void drawPointLightComponent            (ecs::PointLightComponent& component);
    void drawAnimationComponent             (ecs::MeshAnimationComponent& component);
    void drawDirectionalLightComponent      (ecs::DirectionalLightComponent& component);
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

} // gui
} // raekor