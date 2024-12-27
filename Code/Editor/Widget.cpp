#include "pch.h"
#include "Widget.h"
#include "Editor.h"
#include "Profiler.h"
#include "Application.h"

namespace RK {

class Application;

RTTI_DEFINE_TYPE_NO_FACTORY(IWidget) {}

IWidget::IWidget(Editor* inEditor, const String& title) :
	m_Editor(inEditor), m_Title(title)
{
	assert(m_Editor);
}


Entity IWidget::GetActiveEntity() 
{ 
	return m_Editor->GetActiveEntity(); 
}


void IWidget::SetActiveEntity(Entity inEntity) 
{ 
	m_Editor->SetActiveEntity(inEntity); 
}


Scene& IWidget::GetScene() 
{ 
	return *m_Editor->GetScene(); 
}


Assets& IWidget::GetAssets() 
{ 
	return *m_Editor->GetAssets(); 
}


Physics& IWidget::GetPhysics() 
{ 
	return *m_Editor->GetPhysics(); 
}


IRenderInterface& IWidget::GetRenderInterface() 
{ 
	return *m_Editor->GetRenderInterface(); 
}


void Widgets::Draw(float inDeltaTime)
{
	PROFILE_FUNCTION_CPU();

	for (const auto& widget : m_Widgets)
	{
		if (widget->IsOpen())
		{
			ImGuiWindowClass window_class = {};
			window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
			ImGui::SetNextWindowClass(&window_class);

			widget->Draw(this, inDeltaTime);
		}
	}
}


void Widgets::OnEvent(const SDL_Event& inEvent)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
		return;

	for (const auto& widget : m_Widgets)
		if (widget->IsOpen())
			widget->OnEvent(this, inEvent);
}

}