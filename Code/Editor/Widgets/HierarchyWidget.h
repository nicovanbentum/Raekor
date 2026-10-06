#pragma once

#include "Widget.h"

namespace RK {

class Editor;

class HierarchyWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(HierarchyWidget);

	HierarchyWidget(Editor* inEditor);
	virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override;

private:
	void DrawToolbar(Scene& inScene);
	void DrawNode(Scene& inScene, Entity inEntity);
	void DrawFilteredList(Scene& inScene);
	void DrawContextMenu(Scene& inScene, Entity inEntity);
	void DrawCreateMenuItems(Scene& inScene, Entity inParent);

	void HandleSelection(Entity inEntity);
	void HandleDragDrop(Scene& inScene, Entity inEntity);
	void DropTargetWindow(Scene& inScene);

	bool IsSelected(Entity inEntity) const;
	bool IsDescendantOf(Scene& inScene, Entity inEntity, Entity inAncestor) const;
	void Reparent(Scene& inScene, Entity inEntity, Entity inParent);

	const char* GetEntityIcon(Scene& inScene, Entity inEntity) const;

	String m_Filter;
	String m_RenameBuffer;
	Entity m_RenameEntity = Entity::Null;
	Entity m_PrevActiveEntity = Entity::Null;
	bool m_FocusRename = false;
};

}
