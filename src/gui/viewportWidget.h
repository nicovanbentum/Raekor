#pragma once

#include "widget.h"

namespace Raekor {

class Viewport;

class ViewportWidget : public IWidget {
public:
    ViewportWidget(Editor* editor);
    virtual void draw() override;
    virtual void onEvent(const SDL_Event& ev) override;

protected:
    GLuint rendertarget;
    bool gizmoEnabled = true;
    bool mouseInViewport = false;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
};

} // raekor