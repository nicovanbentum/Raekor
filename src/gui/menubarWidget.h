#pragma once

#include "widget.h"

namespace Raekor {

class Scene;

class MenubarWidget : public IWidget {
public:
    MenubarWidget(Editor* editor);
    virtual void draw() override;

private:
    Scene& scene;
    entt::entity& active;
};

}