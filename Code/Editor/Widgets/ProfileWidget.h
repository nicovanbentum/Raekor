#pragma once

#include "Widget.h"
#include "Timer.h"

namespace RK {

class RTTI;
class Editor;
class Application;

class ProfileWidget : public IWidget
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ProfileWidget);

	ProfileWidget(Editor* inEditor);
	void Draw(Widgets* inWidgets, float inDeltaTime) override;
	void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override;
};

} // raekor