#pragma once

#include "widget.h"

namespace Raekor {
    
class AssetsWidget : public IWidget {
    RTTI_CLASS_HEADER(AssetsWidget);
public:
    AssetsWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}
};

}
