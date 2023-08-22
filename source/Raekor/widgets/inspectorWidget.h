#pragma once

#include "widget.h"

namespace Raekor {

class Scene;
struct Name;
struct Node;
struct Mesh;
struct Skeleton;
struct SoftBody;
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
    virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

private:
    void DrawJSONInspector(Widgets* inWidgets);
    void DrawEntityInspector(Widgets* inWidgets);

    void DrawComponent(Name& ioName);
    void DrawComponent(Node& ioNode);
    void DrawComponent(Mesh& ioMesh);
    void DrawComponent(SoftBody& ioSoftBody);
    void DrawComponent(Skeleton& ioSkeleton);
    void DrawComponent(Material& ioMaterial);
    void DrawComponent(Transform& ioTransform);
    void DrawComponent(PointLight& ioPointLight);
    void DrawComponent(BoxCollider& ioBoxCollider);
    void DrawComponent(NativeScript& ioNativeScript);
    void DrawComponent(DirectionalLight& ioDirectionalLight);
};

}

