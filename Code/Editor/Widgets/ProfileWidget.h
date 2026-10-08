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
	struct TrackBounds
	{
		uint32_t mMaxDepth = 0;
		uint64_t mLowestTick = UINT64_MAX;
		uint64_t mHighestTick = 0;
	};

	static TrackBounds sGetTrackBounds(const Array<ProfileSection>& inSections);

	void DrawTrack(int inTrack, const char* inLabel, const Array<ProfileSection>& inSections, float inPixelsPerTick, float inBarHeight, const ImGuiTextFilter& inFilter);

	float m_Zoom = 1.0f;
	String m_FilterInputBuffer;
	int m_SelectedTrack = -1;
	int m_SelectedSectionIndex = -1;
};

} // raekor
