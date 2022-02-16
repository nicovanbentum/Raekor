#pragma once

#include "widget.h"

namespace Raekor {
    
class AssetsWidget : public IWidget {
public:
    TYPE_ID(AssetsWidget);

    AssetsWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}
};

}
