#pragma once

#include "widget.h"

namespace Raekor {

class AssetsWidget : public IWidget
{
	RTTI_DECLARE_TYPE(AssetsWidget);
public:
	AssetsWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}
};

}
