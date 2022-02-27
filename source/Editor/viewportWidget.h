#pragma once

#include "widget.h"

namespace Raekor {

class ViewportWidget : public IWidget {
public:
    TYPE_ID(ViewportWidget);

    ViewportWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override;

    void DisableGizmo() { gizmoEnabled = false; }
    bool IsHovered() { return mouseInViewport; }

protected:
    GLuint rendertarget;
    int rendertargetIndex = 0;
    bool gizmoEnabled = true;
    bool mouseInViewport = false;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
};

} // raekor