#pragma once

#include "widget.h"

namespace Raekor {

class HierarchyWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(HierarchyWidget);

    HierarchyWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    void dropTargetWindow(Scene& scene);
    void dropTargetNode(Scene& scene, Entity entity);

    void drawFamily(Scene& scene, Entity parent, Entity& active);
    bool drawFamilyNode(Scene& scene, Entity entity, Entity& active);
    void drawChildlessNode(Scene& scene, Entity entity, Entity& active);
};

}