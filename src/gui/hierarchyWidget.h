#pragma once

#include "widget.h"

namespace Raekor {

class HierarchyWidget : public IWidget {
public:
    HierarchyWidget(Editor* editor);
    virtual void draw() override;

private:
    void drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active);
    bool drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active);
    void drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active);
};

}