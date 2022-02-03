#pragma once

#include "widget.h"

namespace Raekor {

class Viewport;

class ViewportWidget : public IWidget {
public:
    ViewportWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override;

protected:
    GLuint rendertarget;
    int rendertargetIndex = 0;
    bool gizmoEnabled = true;
    bool mouseInViewport = false;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
};

} // raekor