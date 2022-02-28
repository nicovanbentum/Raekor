#pragma once

#include "widget.h"
#include "Raekor/components.h"

namespace Raekor {

class Scene;

class InspectorWidget : public IWidget {
public:
    TYPE_ID(InspectorWidget);

    InspectorWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    void DrawComponent(Name& component, Entity& active);
    void DrawComponent(Node& component, Entity& active);
    void DrawComponent(Mesh& component, Entity& active);
    void DrawComponent(BoxCollider& component, Entity& active);
    void DrawComponent(Skeleton& component, Entity& active);
    void DrawComponent(Material& component, Entity& active);
    void DrawComponent(Transform& component, Entity& active);
    void DrawComponent(PointLight& component, Entity& active);
    void DrawComponent(NativeScript& component, Entity& active);
    void DrawComponent(DirectionalLight& component, Entity& active);
};

}

