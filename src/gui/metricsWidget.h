#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MetricsWidget : public IWidget {
public:
    MetricsWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    float accumTime = 0.0f;
    std::unordered_map<std::string, float> times;
};

}