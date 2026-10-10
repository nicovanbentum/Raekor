#pragma once

#include "Application.h"

namespace RK {

class Launcher : public Application
{
public:
	Launcher();
	~Launcher();

	virtual void OnUpdate(float inDeltaTime) override;
	virtual void OnEvent(const SDL_Event& inEvent) override;

	bool ShouldLaunch() const { return m_Launch; }
	const Path& GetGameSceneFile() const { return m_GameSceneFile; }

	static Path sGetGameExecutablePath();

private:
	struct GameEntry
	{
		String mName;
		Path mSceneFile;
	};

	void DrawHeader();
	void DrawLaunchTarget();
	void DrawStartupSettings();
	void DrawConsoleVariables();
	void DrawFooter();

	bool DrawLaunchTile(const char* inIcon, const char* inName, bool inSelected, bool inEnabled);

	void SetDisplay(SDL_DisplayID inDisplay);
	void SetGameSceneFile(const Path& inSceneFile);
	void Launch();

	bool m_Launch = false;
	String m_Filter;
	SDL_Renderer* m_Renderer;
	Array<String> m_SortedCvarNames;
	Array<GameEntry> m_Games;
	Path m_GameSceneFile;
	bool m_HasGameExecutable = false;
};

}