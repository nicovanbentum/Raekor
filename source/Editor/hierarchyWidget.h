#pragma once

#include "widget.h"

namespace Raekor {

class HierarchyWidget : public IWidget {
public:
    HierarchyWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    void dropTargetWindow(entt::registry& scene);
    void dropTargetNode(entt::registry& scene, entt::entity entity);

    void drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active);
    bool drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active);
    void drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active);
};

}