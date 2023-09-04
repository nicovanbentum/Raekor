#pragma once

#include "widget.h"

namespace Raekor {

class Renderer;

class RandomWidget : public IWidget
{
public:
	RTTI_DECLARE_TYPE(RandomWidget);

	RandomWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}
};

} // raekor
