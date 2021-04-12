#pragma once

#include "widget.h"

namespace Raekor {

class Viewport;

class ViewportWidget : public IWidget {
public:
    ViewportWidget(Editor* editor);
    virtual void draw() override;

private:
    bool enabled;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
    bool mouseInViewport;
    GLuint* rendertarget;
};

} // raekor