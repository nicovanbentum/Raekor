#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MetricsWidget : public IWidget {
public:
    TYPE_ID(MetricsWidget);

    MetricsWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    float m_UpdateInterval = 0.0f;
    std::unordered_map<std::string, float> m_Times;
};

}