#pragma once

#include "widget.h"

namespace RK {

class Scene;

class MenubarWidget : public IWidget
{
	RTTI_DECLARE_VIRTUAL_TYPE(MenubarWidget);
public:

	MenubarWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}
};

}