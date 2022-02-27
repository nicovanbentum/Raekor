#include "pch.h"
#include "widget.h"
#include "editor.h"

namespace Raekor {

 IWidget::IWidget(Editor* editor, const std::string& title) : 
	 editor(editor), title(title) 
 {
	 assert(editor);
 }

 Scene& IWidget::GetScene() { return editor->m_Scene; }

 Assets& IWidget::GetAssets() { return editor->m_Assets; }

 Physics& IWidget::GetPhysics() { return editor->m_Physics; }

 GLRenderer& IWidget::GetRenderer() { return editor->m_Renderer; }

 entt::entity& IWidget::GetActiveEntity() { return editor->m_ActiveEntity; }


}