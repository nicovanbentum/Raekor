#include "pch.h"
#include "widget.h"
#include "editor.h"

namespace Raekor {

 IWidget::IWidget(Editor* editor, const std::string& title) : editor(editor), title(title) {}

 Scene& IWidget::scene() { return editor->scene; }

 Assets& IWidget::assets() { return editor->assets; }

 GLRenderer& IWidget::renderer() { return editor->renderer; }

}