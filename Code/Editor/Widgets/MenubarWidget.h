#pragma once

#include "widget.h"

namespace RK {

class Scene;
class Editor;

class MenubarWidget : public IWidget
{
	RTTI_DECLARE_VIRTUAL_TYPE(MenubarWidget);
public:

	MenubarWidget(Editor* inEditor);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}

private:
	void DrawFileMenu();
	void DrawEditMenu();
	void DrawViewMenu(Widgets* inWidgets);
	void DrawCreateMenu();
	void DrawPlayMenu();
	void DrawToolsMenu();
	void DrawHelpMenu();

	void DrawShortcutsWindow();
	void DrawAboutWindow();

	void SaveScreenshot();
	void SelectCreatedEntity(Entity inEntity);

	bool m_ShowShortcuts = false;
	bool m_ShowAbout = false;
};

}
