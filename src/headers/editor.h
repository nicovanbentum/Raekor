#pragma once

#include "application.h"
#include "gui.h"
#include "physics.h"
#include "assets.h"
#include "../GUI/widget.h"

namespace Raekor {

class Editor : public Application {
public:
    friend class IWidget;

    Editor();
    virtual ~Editor() = default;

    void onUpdate(float dt) override;
    void onEvent(const SDL_Event& event) override;

    const std::vector<std::shared_ptr<IWidget>>& getWidgets() { return widgets; }

    entt::entity active = entt::null;

private:
    Scene scene;
    Assets assets;
    Physics physics;
    GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;
};

} // raekor