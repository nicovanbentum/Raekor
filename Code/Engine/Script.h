#pragma once

#include "ECS.h"
#include "Scene.h"
#include "Defines.h"
#include "Assets.h"

#define RK_SCRIPT_TYPES_FUNCTION_STR "gGetScriptTypes"

#define RK_REGISTER_SCRIPT(Type) \
	static const bool s##Type##Registered = ( ::RK::gGetModuleScriptTypes().push_back(&RTTI_OF<Type>()), true );

#define RK_SCRIPT_MODULE()                                                                                          \
	::RK::Array<::RK::RTTI*>& ::RK::gGetModuleScriptTypes() { static ::RK::Array<::RK::RTTI*> types; return types; } \
	extern "C" __declspec( dllexport ) const ::RK::Array<::RK::RTTI*>* gGetScriptTypes() { return &::RK::gGetModuleScriptTypes(); }

namespace RK {

class RTTI;

Array<RTTI*>& gGetModuleScriptTypes();

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
    bool HasComponent();

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
bool INativeScript::HasComponent()
{
    return m_Scene->Has<T>(m_Entity);
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


class ScriptModule
{
public:
	using GetTypesFunction = const Array<RTTI*>* (*)();

	ScriptModule() = default;
	~ScriptModule() { Unload(); }

	NO_COPY_NO_MOVE(ScriptModule);

	bool Load(const Path& inModulePath);
	void Unload();

	bool IsLoaded() const { return m_Module != nullptr; }
	bool HasChangedOnDisk() const;

	const Path& GetModulePath() const { return m_ModulePath; }
	Slice<RTTI* const> GetTypes() const { return m_Types; }
	uint32_t GetLoadCount() const { return m_LoadCount; }

	static Path sGetDefaultModulePath();

private:
	Path m_ModulePath;
	Path m_LoadedDirectory;
	void* m_Module = nullptr;
	uint32_t m_LoadCount = 0;
	Array<RTTI*> m_Types;
	fs::file_time_type m_LoadedWriteTime;
};


} // Raekor
