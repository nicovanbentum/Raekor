#pragma once

namespace Raekor::ecs { 
class ECS; 
}

namespace Raekor {

struct Node;
typedef uint32_t Entity;


class NodeSystem {
public:
    static void sAppend(ecs::ECS& registry, Entity parentEntity, Node& parent, Entity childEntity, Node& child);
    static void sRemove(ecs::ECS& registry, Node& node);
    static void sCollapseTransforms(ecs::ECS& registry, Node& node, Entity entity);

    static std::vector<Entity> sGetFlatHierarchy(ecs::ECS& registry, Entity inEntity);
};

}