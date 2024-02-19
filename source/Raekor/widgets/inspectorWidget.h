#pragma once

#include "widget.h"

namespace Raekor {

class RTTI;
class Scene;
class Animation;

struct Name;
struct Node;
struct Mesh;
struct Skeleton;
struct SoftBody;
struct Material;
struct Transform;
struct Light;
struct BoxCollider;
struct NativeScript;
struct DirectionalLight;
struct DDGISceneSettings;

class InspectorWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(InspectorWidget);

	InspectorWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

private:
	void DrawJSONInspector(Widgets* inWidgets);
	void DrawEntityInspector(Widgets* inWidgets);
    void DrawKeyFrameInspector(Widgets* inWidgets);

	void DrawScriptMember(const char* inLabel, int& ioValue);
	void DrawScriptMember(const char* inLabel, bool& ioValue);
	void DrawScriptMember(const char* inLabel, float& ioValue);
	void DrawScriptMember(const char* inLabel, Entity& ioValue);

	void DrawComponent(Entity inEntity, Name& ioName);
	void DrawComponent(Entity inEntity, Node& ioNode);
	void DrawComponent(Entity inEntity, Mesh& ioMesh);
	void DrawComponent(Entity inEntity, Light& ioLight);
	void DrawComponent(Entity inEntity, SoftBody& ioSoftBody);
	void DrawComponent(Entity inEntity, Skeleton& ioSkeleton);
	void DrawComponent(Entity inEntity, Material& ioMaterial);
	void DrawComponent(Entity inEntity, Transform& ioTransform);
	void DrawComponent(Entity inEntity, Animation& ioAnimation);
	void DrawComponent(Entity inEntity, BoxCollider& ioBoxCollider);
	void DrawComponent(Entity inEntity, NativeScript& ioNativeScript);
	void DrawComponent(Entity inEntity, DirectionalLight& ioDirectionalLight);
	void DrawComponent(Entity inEntity, DDGISceneSettings& ioDDGISceneSettings);
};

}

