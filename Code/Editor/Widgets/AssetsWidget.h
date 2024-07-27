#pragma once

#include "widget.h"
#include "cvars.h"

namespace RK {

class Material;
class Animation;

class ComponentsWidget : public IWidget
{
	RTTI_DECLARE_VIRTUAL_TYPE(ComponentsWidget);

	int& m_ComponentIndex = g_CVars.Create("ui_component_index", 0);

public:
	ComponentsWidget(Application* inApp);
	
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}

	bool DrawClickableComponent(Entity inEntity, const Material& inMaterial);
	bool DrawClickableComponent(Entity inEntity, const Animation& inAnimation);
};

}
