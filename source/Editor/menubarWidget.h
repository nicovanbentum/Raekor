#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MenubarWidget : public IWidget {
    RTTI_CLASS_HEADER(MenubarWidget);
public:

    MenubarWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}
};

}