#pragma once

#include "widget.h"
#include "components.h"

namespace Raekor {

class InspectorWidget : public IWidget {
public:
    InspectorWidget(Editor* editor);
    virtual void draw() override;

private:
    void drawComponent(ecs::NameComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::NodeComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MeshComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MaterialComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::TransformComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::PointLightComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::NativeScriptComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::AnimationComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::DirectionalLightComponent& component, entt::registry& scene, entt::entity& active);
};

}

