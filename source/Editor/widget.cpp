#include "pch.h"
#include "widget.h"
#include "editor.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(IWidget) {}

 IWidget::IWidget(Editor* editor, const std::string& title) : 
	 m_Editor(editor), m_Title(title) 
 {
	 assert(editor);
 }

 Scene& IWidget::GetScene() { return m_Editor->m_Scene; }

 Assets& IWidget::GetAssets() { return m_Editor->m_Assets; }

 Physics& IWidget::GetPhysics() { return m_Editor->m_Physics; }

 GLRenderer& IWidget::GetRenderer() { return m_Editor->m_Renderer; }

 entt::entity& IWidget::GetActiveEntity() { return m_Editor->m_ActiveEntity; }


}