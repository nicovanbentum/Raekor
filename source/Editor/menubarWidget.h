#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MenubarWidget : public IWidget {
    RTTI_CLASS_HEADER(MenubarWidget);
public:

    MenubarWidget(Application* inApp);
    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override {}
};

}