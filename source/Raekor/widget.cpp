#include "pch.h"
#include "widget.h"
#include "application.h"

namespace Raekor {

class Application;

RTTI_DEFINE_TYPE_NO_FACTORY(IWidget) {}

 IWidget::IWidget(Application* inApp, const std::string& title) : 
	 m_Editor(inApp), m_Title(title)
 {
	 assert(m_Editor);
 }

 Entity IWidget::GetActiveEntity() { return m_Editor->GetActiveEntity(); }

 void IWidget::SetActiveEntity(Entity inEntity) { m_Editor->SetActiveEntity(inEntity); }

 Scene& IWidget::GetScene() { return *m_Editor->GetScene(); }

 Assets& IWidget::GetAssets() { return *m_Editor->GetAssets(); }

 Physics& IWidget::GetPhysics() { return *m_Editor->GetPhysics(); }

 IRenderer& IWidget::GetRenderer() { return *m_Editor->GetRenderer(); }


 void Widgets::Draw(float inDeltaTime) {
     for (const auto& widget : m_Widgets) {
         if (widget->IsOpen()) {
             auto window_class = ImGuiWindowClass();
             window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
             ImGui::SetNextWindowClass(&window_class);

             widget->Draw(this, inDeltaTime);
         }
     }
 }


 void Widgets::OnEvent(const SDL_Event& inEvent) {
     if (ImGui::GetIO().WantCaptureKeyboard)
         return;

     for (const auto& widget : m_Widgets)
         if (widget->IsOpen())
             widget->OnEvent(this, inEvent);
 }

}