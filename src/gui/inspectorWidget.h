#pragma once

#include "widget.h"
#include "components.h"

namespace Raekor {

class InspectorWidget : public IWidget {
public:
    InspectorWidget(Editor* editor);
    virtual void draw() override;

private:
    void drawComponent(Name& component, entt::registry& scene, entt::entity& active);
    void drawComponent(Node& component, entt::registry& scene, entt::entity& active);
    void drawComponent(Mesh& component, entt::registry& scene, entt::entity& active);
    void drawComponent(Material& component, entt::registry& scene, entt::entity& active);
    void drawComponent(Transform& component, entt::registry& scene, entt::entity& active);
    void drawComponent(PointLight& component, entt::registry& scene, entt::entity& active);
    void drawComponent(NativeScriptComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(Skeleton& component, entt::registry& scene, entt::entity& active);
    void drawComponent(DirectionalLight& component, entt::registry& scene, entt::entity& active);
};

}

