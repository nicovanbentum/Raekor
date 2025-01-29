#pragma once

#include "ECS.h"
#include "Scene.h"
#include "Assets.h"

#define DECLARE_SCRIPT_CLASS(T) class T : public INativeScript { public: RTTI_DECLARE_VIRTUAL_TYPE(T); };

#define SCRIPT_EXPORTED_FUNCTION_STR "gGetTypes"
#define SCRIPT_EXPORTED_FUNCTION_NAME gGetTypes

namespace RK {

class Input;
class Scene;
class Camera;
class Physics;
class Application;
class DebugRenderer;
class IRenderInterface;

class INativeScript
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(INativeScript);

	friend class Scene;

	typedef int ( __cdecl* RegisterFn ) ( RTTI** );

	virtual ~INativeScript() = default;

	virtual void OnBind() {};

	virtual void OnStart() {};

	virtual void OnStop() {};

	virtual void OnUpdate(float inDeltaTime) = 0;

	virtual void OnEvent(const SDL_Event& inEvent) = 0;

    RK::Entity GetEntity() const { return m_Entity; }

	// helper functions to interface with the engine
	Scene* GetScene();
	Assets* GetAssets();
	Physics* GetPhysics();
	IRenderInterface* GetRenderInterface();

	template<typename T>
	T& GetComponent();

	template<typename T>
	T* FindComponent();

	template<typename T>
	T* FindComponent(Entity inEntity);

	template<typename T> requires std::derived_from<T, Asset>
	T* GetAsset(const Path& inPath);

	void Log(const String& inText);

protected:
	RK::Entity m_Entity;
	RK::Input* m_Input = nullptr;
	RK::Scene* m_Scene = nullptr;
	RK::Application* m_App = nullptr;
	RK::DebugRenderer* m_DebugRenderer = nullptr;
};


template<typename T>
T& INativeScript::GetComponent()
{
	return m_Scene->Get<T>(m_Entity);
}

template<typename T>
T* INativeScript::FindComponent()
{
	return m_Scene->GetPtr<T>(m_Entity);
}

template<typename T>
T* INativeScript::FindComponent(Entity inEntity)
{
	return m_Scene->GetPtr<T>(inEntity);
}

template<typename T> requires std::derived_from<T, Asset>
T* INativeScript::GetAsset(const Path& inPath)
{
	return GetAssets()->GetAsset<T>(inPath);
}


} // Raekor
