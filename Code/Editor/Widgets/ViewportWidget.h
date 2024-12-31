#pragma once

#include "Widget.h"
#include "Undo.h"

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
	bool IsHovered() const { return m_IsMouseOver; }
	void DisableGizmo() { m_IsGizmoEnabled = false; }

protected:
	void AddClickableQuad(const Viewport& inViewport, Entity inEntity, ImTextureID inTexture, Vec3 inPos, float inSize);

	float m_TotalTime = 0;
	bool m_Changed = false;

	int m_RenderTargetIndex = 0;
	uint64_t m_DisplayTexture = 0;

	bool m_IsMouseOver = false;
	bool m_IsGizmoEnabled = true;
	bool m_IsUsingGizmo = false;
	bool m_WasUsingGizmo = false;

	ComponentUndo<Transform> m_TransformUndo;
	ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;

	std::vector<ClickableQuad> m_EntityQuads;
};

} // raekor