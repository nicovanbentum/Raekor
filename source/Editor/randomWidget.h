#pragma once

#include "widget.h"

namespace Raekor {

class GLRenderer;

class RandomWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(RandomWidget);

    RandomWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}
};

} // raekor
