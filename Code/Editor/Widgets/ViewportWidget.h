#pragma once

#include "Widget.h"
#include "CVars.h"
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
    static constexpr ImVec4 cRunningColor = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
    static constexpr ImVec4 cPausedColor = ImVec4(0.35f, 0.78f, 1.00f, 1.00f);
    static constexpr ImVec4 cStoppedColor = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);

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

	ImVec2 m_WindowPos;
	ImVec2 m_WindowSize;

	ComponentUndo<Transform> m_TransformUndo;
	ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;

	std::vector<ClickableQuad> m_EntityQuads;
};

} // raekor