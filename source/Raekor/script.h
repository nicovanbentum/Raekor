#pragma once

#include "ecs.h"
#include "scene.h"

#define DECLARE_SCRIPT_CLASS(T) class T : public INativeScript { public: RTTI_DECLARE_TYPE(T); };

#define SCRIPT_EXPORTED_FUNCTION_STR "gGetTypes"
#define SCRIPT_EXPORTED_FUNCTION_NAME gGetTypes

namespace Raekor {

class Input;
class Scene;
class Camera;
class Application;
class DebugRenderer;

class INativeScript
{
public:
	RTTI_DECLARE_TYPE(INativeScript);

	friend class Scene;

	typedef int ( __cdecl* RegisterFn ) ( RTTI** );

	virtual ~INativeScript() = default;

	virtual void OnBind() {};

	virtual void OnStart() {};

	virtual void OnStop() {};

	virtual void OnUpdate(float inDeltaTime) = 0;

	virtual void OnEvent(const SDL_Event& inEvent) = 0;

	// helper functions to interface with the engine
	template<typename T>
	T& GetComponent();

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


} // Raekor
