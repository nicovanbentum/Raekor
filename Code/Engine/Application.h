#pragma once

#include "ECS.h"
#include "RTTI.h"
#include "Assets.h"
#include "Camera.h"
#include "Script.h"
#include "Defines.h"
#include "DiscordRPC.h"
#include "RenderInterface.h"

namespace RK {

class Scene;
class Assets;
class Physics;
class IWidget;
class UndoSystem;
class Application;
class IRenderInterface;

struct ConfigSettings
{
	RTTI_DECLARE_TYPE(ConfigSettings);

	bool mShowUI = true;
	int mDisplayID = 0;
	bool mVsyncEnabled = true;
	String mAppName = "RK Renderer";
	Path mFontFile = "Assets/Fonts/Inter-Medium.ttf";
	Path mSceneFile = "";
	Array<Path> mRecentScenes;
};


enum WindowFlag
{
	NONE = 0,
	HIDDEN = SDL_WINDOW_HIDDEN,
	RESIZE = SDL_WINDOW_RESIZABLE,
	OPENGL = SDL_WINDOW_OPENGL,
	VULKAN = SDL_WINDOW_VULKAN,
	BORDERLESS = SDL_WINDOW_BORDERLESS,
};
using WindowFlags = uint32_t;


enum EGameState
{
	GAME_RUNNING,
	GAME_PAUSED,
	GAME_STOPPED
};


enum IPC
{
	LOG_MESSAGE_SENT = 1,
	LOG_MESSAGE_RECEIVED = 2,
};


class Application
{
    RTTI_DECLARE_VIRTUAL_TYPE(Application);

protected:
	Application(WindowFlags inFlags);

public:
	virtual ~Application();

	void Run();
	void Terminate() { m_Running = false; }

	virtual void OnUpdate(float dt) = 0;
	virtual void OnEvent(const SDL_Event& event) = 0;
	virtual bool OnCloseRequested() { return true; }
	virtual bool IsPausedWhenMinimized() const { return true; }

	bool IsWindowBorderless() const;
	bool IsWindowExclusiveFullscreen() const;

	virtual Scene* GetScene() { return nullptr; }
	virtual Assets* GetAssets() { return nullptr; }
	virtual Physics* GetPhysics() { return nullptr; }
	virtual UndoSystem* GetUndo() { return nullptr; }
	virtual IRenderInterface* GetRenderInterface() { return nullptr; }

	virtual void SetActiveEntity(Entity inEntity) {}
	virtual Entity GetActiveEntity() const { return Entity::Null; }

    virtual void SetCameraEntity(Entity inEntity) {}
    virtual Entity GetCameraEntity() const { return Entity::Null; }

	void SetGameState(EGameState inState) { m_GameState = inState; }
	EGameState GetGameState() const { return m_GameState; }

	uint64_t GetFrameCounter() const { return m_FrameCounter; }
	const ConfigSettings& GetConfigSettings() const { return m_ConfigSettings; }

	void AddRecentScene(const Path& inPath);

	SDL_Window* GetWindow() { return m_Window; }
	const SDL_Window* GetWindow() const { return m_Window; }

	Viewport& GetViewport() { return m_Viewport; }
	const Viewport& GetViewport() const { return m_Viewport; }

	DiscordRPC& GetDiscordRPC() { return m_DiscordRPC; }
	const DiscordRPC& GetDiscordRPC() const { return m_DiscordRPC; }

protected:
	bool m_Running = true;
	EGameState m_GameState = GAME_STOPPED;
	uint64_t m_FrameCounter = 0;
	SDL_Window* m_Window = nullptr;

	Viewport m_Viewport;
	DiscordRPC m_DiscordRPC;
	ConfigSettings m_ConfigSettings;
};


class Game : public Application
{
public:
    Game(WindowFlags inFlags);
    virtual ~Game() = default;

    virtual void Start();
    virtual void Stop();

    virtual void Pause();
    virtual void Unpause();

    bool LoadScripts(const Path& inModulePath = ScriptModule::sGetDefaultModulePath());
    void UnloadScripts();
    void ReloadScripts();
    bool ReloadScriptsIfChanged(float inDeltaTime);

    const ScriptModule& GetScriptModule() const { return m_ScriptModule; }

protected:
    void CallScripts(void ( INativeScript::* inFunction )( ), const char* inFunctionName);

    ScriptModule m_ScriptModule;
    float m_ScriptPollTime = 0.0f;
};


} // Namespace Raekor