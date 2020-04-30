#pragma once

#include "ecs.h"

namespace Raekor {
namespace GUI {

class InspectorWindow {
public:
    static void draw(ECS::Scene& scene, ECS::Entity entity);

private:
    static void drawNameComponent(ECS::NameComponent* component);
    static void drawTransformComponent(ECS::TransformComponent* component);
    static void drawMeshComponent(ECS::MeshComponent* component);
    static void drawMeshRendererComponent(ECS::MeshRendererComponent* component);
    static void drawMaterialComponent(ECS::MaterialComponent* component);
};

} // gui
} // raekor