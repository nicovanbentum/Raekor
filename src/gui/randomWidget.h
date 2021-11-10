#pragma once

#include "widget.h"

namespace Raekor {

class GLRenderer;

class RandomWidget : public IWidget {
public:
    RandomWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}
};

} // raekor
