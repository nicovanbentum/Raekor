#pragma once

#include "Widget.h"
#include "Editor.h"
#include "Undo.h"

namespace RK {

class RTTI;
class Scene;
class Camera;
class Editor;
class Animation;

struct Name;
struct Mesh;
struct Skeleton;
struct SoftBody;
struct Material;
struct Transform;
struct Light;
struct RigidBody;
struct NativeScript;
struct DirectionalLight;
struct DDGISceneSettings;

class InspectorWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(InspectorWidget);

	InspectorWidget(Editor* inEditor);
	virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

	bool SceneChanged() const { return m_SceneChanged; }

private:
	void DrawJSONInspector(Widgets* inWidgets);
	bool DrawEntityInspector(Widgets* inWidgets);
    void DrawKeyFrameInspector(Widgets* inWidgets);

	void DrawScriptMember(const char* inLabel, int& ioValue);
	void DrawScriptMember(const char* inLabel, bool& ioValue);
	void DrawScriptMember(const char* inLabel, float& ioValue);
	void DrawScriptMember(const char* inLabel, Entity& ioValue);

	bool DrawComponent(Entity inEntity, Name& ioName);
	bool DrawComponent(Entity inEntity, Mesh& ioMesh);
	bool DrawComponent(Entity inEntity, Light& ioLight);
	bool DrawComponent(Entity inEntity, Camera& ioCamera);
	bool DrawComponent(Entity inEntity, SoftBody& ioSoftBody);
	bool DrawComponent(Entity inEntity, Skeleton& ioSkeleton);
	bool DrawComponent(Entity inEntity, Material& ioMaterial);
	bool DrawComponent(Entity inEntity, Transform& ioTransform);
	bool DrawComponent(Entity inEntity, Animation& ioAnimation);
	bool DrawComponent(Entity inEntity, RigidBody& ioBoxCollider);
	bool DrawComponent(Entity inEntity, NativeScript& ioNativeScript);
	bool DrawComponent(Entity inEntity, DirectionalLight& ioDirectionalLight);
	bool DrawComponent(Entity inEntity, DDGISceneSettings& ioDDGISceneSettings);

	template<typename T>
	void CheckForUndo(Entity inEntity, T& inComponent, ComponentUndo<T>& inUndo);

private:
	bool m_SceneChanged = false;
	ComponentUndo<Name> m_NameUndo;
	ComponentUndo<Light> m_LightUndo;
	ComponentUndo<Camera> m_CameraUndo;
	ComponentUndo<Material> m_MaterialUndo;
	ComponentUndo<Transform> m_TransformUndo;
	ComponentUndo<DirectionalLight> m_DirectionalLightUndo;
	ComponentUndo<DDGISceneSettings> m_DDGISceneSettingsUndo;
};

template<typename T>
void InspectorWidget::CheckForUndo(Entity inEntity, T& inComponent, ComponentUndo<T>& inUndo)
{
	if (ImGui::IsItemActivated())
	{
		inUndo.entity = inEntity;
		inUndo.previous = inComponent;
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		inUndo.current = inComponent;
		m_Editor->GetUndo()->PushUndo(inUndo);
	}
}

}

