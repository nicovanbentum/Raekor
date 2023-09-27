#pragma once

#include "ecs.h"
#include "scene.h"

#define DEFINE_SCRIPT_CLASS(x) extern "C" __declspec(dllexport) Raekor::INativeScript * __cdecl Create##x() { return new class x(); }

namespace Raekor {

class Input;

/* Example

class MoveCubeScript : public NativeScript {
public:
	void OnUpdate(float inDeltaTime) {};
	void OnEvent(const SDL_Event& inEvent) {};

};

*/

class INativeScript
{
public:
	typedef INativeScript* ( __cdecl* FactoryType )( );

	virtual ~INativeScript() = default;

	void Bind(Raekor::Entity inEntity, Raekor::Scene* inScene);

	virtual void OnUpdate(float inDeltaTime) = 0;

	virtual void OnEvent(const SDL_Event& inEvent) = 0;

	// helper functions to interface with the engine
	template<typename T>
	T& GetComponent();

private:
	Raekor::Scene* m_Scene = nullptr;
	Raekor::Entity m_Entity;
};


template<typename T>
T& INativeScript::GetComponent()
{
	return m_Scene->Get<T>(m_Entity);
}


SCRIPT_INTERFACE Input* GetInput();


} // Raekor
