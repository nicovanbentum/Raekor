#pragma once

#include "widget.h"

namespace Raekor {
    
class AssetsWidget : public IWidget {
    RTTI_CLASS_HEADER(AssetsWidget);
public:
    AssetsWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}
};

}
