#pragma once

#include "widget.h"

namespace Raekor {

class ViewportWidget : public IWidget
{
public:
	RTTI_DECLARE_TYPE(ViewportWidget);

	ViewportWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	bool Changed() const { return m_Changed; }
	bool IsHovered() const { return mouseInViewport; }

	void DisableGizmo() { gizmoEnabled = false; }

protected:
	bool m_Changed = false;
	float m_TotalTime = 0;
	uint64_t m_DisplayTexture;
	int m_RenderTargetIndex = 0;
	bool gizmoEnabled = true;
	bool mouseInViewport = false;
	ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
};

} // raekor