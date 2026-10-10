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
	const Path& GetSceneFile() const { return m_SceneFile; }

private:
	struct GameEntry
	{
		String mName;
		Path mSceneFile;
	};

	void DrawHeader();
	void DrawGames();
	void DrawStartupSettings();
	void DrawConsoleVariables();
	void DrawFooter();

	bool DrawGameTile(const GameEntry& inGame, bool inSelected);

	void SetDisplay(SDL_DisplayID inDisplay);
	void SetSceneFile(const Path& inSceneFile);
	void Launch();

	bool m_Launch = false;
	String m_Filter;
	SDL_Renderer* m_Renderer;
	Array<String> m_SortedCvarNames;
	Array<GameEntry> m_Games;
	Path m_SceneFile;
};

}