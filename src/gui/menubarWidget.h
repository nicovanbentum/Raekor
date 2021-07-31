#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MenubarWidget : public IWidget {
public:
    MenubarWidget(Editor* editor);
    virtual void draw() override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    entt::entity& active;
};

}