#pragma once

#include "widget.h"
#include "components.h"

namespace Raekor {

class Scene;

class InspectorWidget : public IWidget {
public:
    InspectorWidget(Editor* editor);
    virtual void draw() override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    void drawComponent(Name& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(Node& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(Mesh& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(Skeleton& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(Material& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(Transform& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(PointLight& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(NativeScript& component, Assets& assets, Scene& scene, entt::entity& active);
    void drawComponent(DirectionalLight& component, Assets& assets, Scene& scene, entt::entity& active);
};

}

