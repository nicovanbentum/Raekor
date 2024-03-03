#pragma once

#include "widget.h"
#include "timer.h"

namespace Raekor {

class RTTI;
class Application;

class ProfileWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ProfileWidget);

	ProfileWidget(Application* inApp);
	void Draw(Widgets* inWidgets, float inDeltaTime) override;
	void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override;
};

} // raekor