#include "pch.h"
#include "widget.h"
#include "editor.h"

namespace Raekor {

 IWidget::IWidget(Editor* editor, const std::string& title) : 
	 editor(editor), title(title) 
 {
	 assert(editor);
 }

 Scene& IWidget::GetScene() { return editor->scene; }

 Assets& IWidget::GetAssets() { return editor->assets; }

 GLRenderer& IWidget::GetRenderer() { return editor->renderer; }

}