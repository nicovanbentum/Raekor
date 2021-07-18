#pragma once

#include "application.h"
#include "gui.h"
#include "physics.h"
#include "assets.h"
#include "../GUI/widget.h"

namespace Raekor {

class Editor : public WindowApplication {
public:
    friend class IWidget;

    Editor();
    virtual ~Editor() = default;
    
    virtual void update(float dt);
    virtual void onEvent(const SDL_Event& event);

    const std::vector<std::shared_ptr<IWidget>>& getWidgets() { return widgets; }

    entt::entity active = entt::null;

private:
    Scene scene;
    Assets assets;
    GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;

    Physics physics;
    bool inAltMode = false;
};

} // raekor