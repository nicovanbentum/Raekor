#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MetricsWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(MetricsWidget);

    MetricsWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}

private:
    float m_UpdateInterval = 0.0f;
    std::unordered_map<std::string, float> m_Times;
};

}