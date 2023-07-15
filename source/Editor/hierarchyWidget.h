#pragma once

#include "widget.h"

namespace Raekor {

class HierarchyWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(HierarchyWidget);

    HierarchyWidget(Application* inApp);
    virtual void Draw(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}

private:
    void dropTargetWindow(Scene& scene);
    void dropTargetNode(Scene& scene, Entity entity);

    void drawFamily(Scene& scene, Entity parent, Entity& active);
    bool drawFamilyNode(Scene& scene, Entity entity, Entity& active);
    void drawChildlessNode(Scene& scene, Entity entity, Entity& active);
};

}