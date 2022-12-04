#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MenubarWidget : public IWidget {
    RTTI_CLASS_HEADER(MenubarWidget);
public:

    MenubarWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    entt::entity& m_ActiveEntity;
};

}