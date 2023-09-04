#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MetricsWidget : public IWidget
{
public:
	RTTI_DECLARE_TYPE(MetricsWidget);

	MetricsWidget(Application* inApp);
	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}

private:
	float m_UpdateInterval = 0.0f;
	std::unordered_map<std::string, float> m_Times;
};

}