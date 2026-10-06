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

private:
	void DrawHeader();
	void DrawStartupSettings();
	void DrawConsoleVariables();
	void DrawFooter();

	void SetDisplay(SDL_DisplayID inDisplay);
	void Launch();

	bool m_Launch = false;
	String m_Filter;
	SDL_Renderer* m_Renderer;
	Array<String> m_SortedCvarNames;
};

}