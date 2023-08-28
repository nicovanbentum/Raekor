#pragma once

#include "widget.h"

namespace Raekor {

class HierarchyWidget : public IWidget {
public:
    RTTI_DECLARE_TYPE(HierarchyWidget);

    HierarchyWidget(Application* inApp);
    virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

private:
    void DropTargetWindow(Scene& inScene);
    void DropTargetNode(Scene& inScene, Entity inEntity);

    void DrawFamily(Scene& inScene, Entity inParent, Entity& inActive);
    bool DrawFamilyNode(Scene& inScene, Entity inEntity, Entity& inActive);
    void DrawChildlessNode(Scene& inScene, Entity inEntity, Entity& inActive);
};

}