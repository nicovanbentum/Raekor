#pragma once

#include "Widget.h"
#include "Timer.h"
#include "Profiler.h"

namespace RK {

class RTTI;
class Editor;
class Application;

class ProfileWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ProfileWidget);

	ProfileWidget(Editor* inEditor);
	void Draw(Widgets* inWidgets, float inDeltaTime) override;
	void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override;

private:
	void SetShowGPU(bool inShowGPU);

	float m_Zoom = 1.0f;
	bool m_ShowGPU = true;
	String m_FilterInputBuffer;
	int m_SelectedSectionIndex = -1;
};

} // raekor
