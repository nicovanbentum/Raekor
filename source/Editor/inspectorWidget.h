#pragma once

#include "widget.h"

namespace Raekor {

class Scene;
struct Name;
struct Node;
struct Mesh;
struct Skeleton;
struct Material;
struct Transform;
struct PointLight;
struct BoxCollider;
struct NativeScript;
struct DirectionalLight;

class InspectorWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(InspectorWidget);

    InspectorWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}

private:
    void DrawJSONInspector();
    void DrawEntityInspector();

    void DrawComponent(Name& component);
    void DrawComponent(Node& component);
    void DrawComponent(Mesh& component);
    void DrawComponent(Skeleton& component);
    void DrawComponent(Material& component);
    void DrawComponent(Transform& component);
    void DrawComponent(PointLight& component);
    void DrawComponent(BoxCollider& component);
    void DrawComponent(NativeScript& component);
    void DrawComponent(DirectionalLight& component);
};

}

