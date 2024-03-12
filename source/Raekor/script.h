#pragma once

#include "ecs.h"
#include "scene.h"

#define DECLARE_SCRIPT_CLASS(T) class T : public INativeScript { public: RTTI_DECLARE_VIRTUAL_TYPE(T); };

#define SCRIPT_EXPORTED_FUNCTION_STR "gGetTypes"
#define SCRIPT_EXPORTED_FUNCTION_NAME gGetTypes

namespace Raekor {

class Input;
class Scene;
class Asset;
class Assets;
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

	// helper functions to interface with the engine
	Scene* GetScene();
	Assets* GetAssets();
	Physics* GetPhysics();
	IRenderInterface* GetRenderInterface();

	template<typename T>
	T& GetComponent();

	template<typename T>
	T* TryGetComponent();

	template<typename T>
	T* FindComponent(Entity inEntity);

	template<typename T> requires std::derived_from<T, Asset>
	T* GetAsset(const Path& inPath);

	void Log(const std::string& inText);

protected:
	Raekor::Entity m_Entity;
	Raekor::Input* m_Input = nullptr;
	Raekor::Scene* m_Scene = nullptr;
	Raekor::Camera* m_Camera = nullptr;
	Raekor::Application* m_App = nullptr;
	Raekor::DebugRenderer* m_DebugRenderer = nullptr;
};


template<typename T>
T& INativeScript::GetComponent()
{
	return m_Scene->Get<T>(m_Entity);
}

template<typename T>
T* INativeScript::TryGetComponent()
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
