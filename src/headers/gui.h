#pragma once

#include "ecs.h"

namespace Raekor {
namespace GUI {

class InspectorWindow {
public:
    void draw(ECS::Scene& scene, ECS::Entity entity);

private:

    void drawNameComponent(ECS::NameComponent* component);
    void drawTransformComponent(ECS::TransformComponent* component);
    void drawMeshComponent(ECS::MeshComponent* component);
    void drawMeshRendererComponent(ECS::MeshRendererComponent* component);
    void drawMaterialComponent(ECS::MaterialComponent* component);
};

} // gui
} // raekor