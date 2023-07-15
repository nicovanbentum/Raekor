#pragma once

#include "widget.h"

namespace Raekor {

class ViewportWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(ViewportWidget);

    ViewportWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override;

    void DisableGizmo() { gizmoEnabled = false; }
    bool IsHovered() { return mouseInViewport; }

protected:
    uint64_t m_DisplayTexture;
    int rendertargetIndex = 0;
    bool gizmoEnabled = true;
    bool mouseInViewport = false;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
};

} // raekor