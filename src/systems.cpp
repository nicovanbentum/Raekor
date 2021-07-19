#include "pch.h"
#include "systems.h"

namespace Raekor {

void NodeSystem::append(entt::registry& registry, Node& parent, Node& child) {
    auto view = registry.view<Node>();
    child.parent = entt::to_entity(registry, parent);

    // if its the parent's first child we simply assign it
    if (parent.firstChild == entt::null) {
        parent.firstChild = entt::to_entity(registry, child);
        return;
    } else {
        // ensure we can get the first child's node
        auto lastChild = parent.firstChild;

        // traverse to the end of the sibling chain
        while (view.get<Node>(lastChild).nextSibling != entt::null) {
            lastChild = view.get<Node>(lastChild).nextSibling;
        }

        view.get<Node>(lastChild).nextSibling = entt::to_entity(registry, child);
        child.prevSibling = lastChild;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void NodeSystem::remove(entt::registry& registry, Node& node) {
    // decrement parent's child count
    if (node.parent == entt::null) return;
    auto& parent = registry.get<Node>(node.parent);

    // handle first child case
    if (entt::to_entity(registry, node) == parent.firstChild) {
        parent.firstChild = node.nextSibling;
     // other cases
    } else {
        auto& prevComp = registry.get<Node>(node.prevSibling); 
        prevComp.nextSibling = node.nextSibling;

        if (node.nextSibling != entt::null) {
            auto& nextComp = registry.get<Node>(node.nextSibling);
            nextComp.prevSibling = node.prevSibling;
        }

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<entt::entity> NodeSystem::getFlatHierarchy(entt::registry& registry, Node& startingNode) {
    std::vector<entt::entity> result;
    std::queue<entt::entity> entities;

    entities.push(entt::to_entity(registry, startingNode));

    while (!entities.empty()) {
        auto& current = registry.get<Node>(entities.front());
        result.push_back(entities.front());
        entities.pop();

        // if it has children
        if (current.firstChild != entt::null) {
            for (auto it = current.firstChild; it != entt::null; it = registry.get<Node>(it).nextSibling) {
                entities.push(it);
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