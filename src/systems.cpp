#include "pch.h"
#include "systems.h"

namespace Raekor {

void NodeSystem::append(entt::registry& registry, ecs::NodeComponent& parent, ecs::NodeComponent& child) {
    auto view = registry.view<ecs::NodeComponent>();
    child.parent = entt::to_entity(registry, parent);
    parent.childCount += 1;

    // if its the parent's first child we simply assign it
    if (parent.firstChild == entt::null) {
        parent.firstChild = entt::to_entity(registry, child);
        return;
    } else {
        // ensure we can get the first child's node
        auto lastChild = parent.firstChild;

        // traverse to the end of the sibling chain
        while (view.get<ecs::NodeComponent>(lastChild).nextSibling != entt::null) {
            lastChild = view.get<ecs::NodeComponent>(lastChild).nextSibling;
        }

        view.get<ecs::NodeComponent>(lastChild).nextSibling = entt::to_entity(registry, child);
        child.prevSibling = lastChild;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void NodeSystem::remove(entt::registry& registry, ecs::NodeComponent& node) {
    // decrement parent's child count
    if (node.parent == entt::null) return;
    auto& parent = registry.get<ecs::NodeComponent>(node.parent);
    parent.childCount -= 1;

    // handle first child case
    if (entt::to_entity(registry, node) == parent.firstChild) {
        parent.firstChild = node.nextSibling;
     // other cases
    } else {
        auto& prevComp = registry.get<ecs::NodeComponent>(node.prevSibling); 
        prevComp.nextSibling = node.nextSibling;

        if (node.nextSibling != entt::null) {
            auto& nextComp = registry.get<ecs::NodeComponent>(node.nextSibling);
            nextComp.prevSibling = node.prevSibling;
        }

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<entt::entity> NodeSystem::getTree(entt::registry& registry, ecs::NodeComponent& node) {
    std::vector<entt::entity> result;
    std::queue<entt::entity> entities;

    entities.push(entt::to_entity(registry, node));

    while (!entities.empty()) {
        auto& current = registry.get<ecs::NodeComponent>(entities.front());
        result.push_back(entities.front());
        entities.pop();

        // if it has children
        if (current.childCount > 0) {
            // start iteration from the first child
            entities.push(current.firstChild);
            auto& childNode = registry.get<ecs::NodeComponent>(current.firstChild);

            // start at 1 to skip first child
            for (int i = 1; i < current.childCount; i++) {
                entities.push(childNode.nextSibling);
                childNode = registry.get<ecs::NodeComponent>(childNode.nextSibling);
            }
        }

    }

    // reverse so iteration starts at leaves
    std::reverse(result.begin(), result.end());

    // remove the original node entity
    result.pop_back();

    return result;
}



} // raekor