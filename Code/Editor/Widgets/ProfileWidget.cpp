#include "PCH.h"
#include "ProfileWidget.h"

#include "Timer.h"
#include "Editor.h"
#include "Profiler.h"
#include "Application.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ProfileWidget) {}


ProfileWidget::ProfileWidget(Editor* inEditor) : IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_RULER " Profiler " )) {}


void ProfileWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	for (const CPUProfileSection& section : g_Profiler->GetCPUProfileSections())
	{
		assert(section.mEndTick); // make sure we're displaying profiled sections have actually finished

		const float time = Timer::sGetTicksToSeconds(section.mEndTick - section.mStartTick);

		for (int depth = 0; depth < section.mDepth; depth++)
		{
			ImGui::Text("  ");
			ImGui::SameLine();
		}

		ImGui::Text("%s : %.2f ms", section.mName, Timer::sToMilliseconds(section.GetSeconds()));
	}

	ImGui::End();
}


void ProfileWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) 
{
	if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
	{
		switch (inEvent.key.keysym.sym)
		{
			case SDLK_SPACE:
			{
				g_Profiler->SetEnabled(!g_Profiler->IsEnabled());
			} break;
		}
	}
}

} // raekor