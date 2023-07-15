#include "pch.h"
#include "widget.h"
#include "editor.h"

namespace Raekor {

class Application;

RTTI_CLASS_CPP_NO_FACTORY(IWidget) {}

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

}