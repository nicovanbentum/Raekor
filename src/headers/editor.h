#pragma once

#include "application.h"
#include "gui.h"
#include "physics.h"
#include "assets.h"
#include "../GUI/widget.h"

namespace Raekor {

class Editor : public WindowApplication {
public:
    Editor();
    virtual ~Editor() = default;
    
    virtual void update(float dt);
    virtual void onEvent(const SDL_Event& event);

    Scene scene;
    GLRenderer renderer;
    AssetManager assetManager;
    entt::entity active = entt::null;

    std::vector<std::shared_ptr<IWidget>> widgets;
private:
    Physics physics;
    bool inAltMode = false;
};

} // raekor