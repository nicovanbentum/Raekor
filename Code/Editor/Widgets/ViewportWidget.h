#pragma once

#include "widget.h"

namespace RK {

class Editor;

struct ClickableQuad
{
	Entity mEntity;
	std::array<Vec4, 4> mVertices;
};

class ViewportWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ViewportWidget);

	ViewportWidget(Editor* inEditor);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

	bool Changed() const { return m_Changed; }
	bool IsHovered() const { return mouseInViewport; }

	void DisableGizmo() { gizmoEnabled = false; }

protected:
	void AddClickableQuad(const Viewport& inViewport, Entity inEntity, ImTextureID inTexture, Vec3 inPos, float inSize);

	bool m_Changed = false;
	float m_TotalTime = 0;
	uint64_t m_DisplayTexture;
	int m_RenderTargetIndex = 0;
	bool gizmoEnabled = true;
	bool mouseInViewport = false;
	ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
	std::vector<ClickableQuad> m_EntityQuads;
};

} // raekor