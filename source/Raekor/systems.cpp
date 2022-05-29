#include "pch.h"
#include "systems.h"

namespace Raekor {

void NodeSystem::sAppend(entt::registry& registry, Node& parent, Node& child) {
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
        while (view.get<Node>(lastChild).nextSibling != entt::null)
            lastChild = view.get<Node>(lastChild).nextSibling;

        view.get<Node>(lastChild).nextSibling = entt::to_entity(registry, child);
        child.prevSibling = lastChild;
    }
}


void NodeSystem::sRemove(entt::registry& registry, Node& node) {
    for (auto it = node.firstChild; it != entt::null; it = registry.get<Node>(it).nextSibling) {
        auto& child = registry.get<Node>(it);
        // this will either hook it up to the node's parent or set it to null, 
        // node's parent is the desired outcome when collapsing transforms
        child.parent = node.parent;
    }

    // handle first child case, example: node | sibling1 | sibling2 | etc.
    if (node.prevSibling == entt::null) {
        // set sibling1's previous sibling to null (since it is now the first child)
        if (node.nextSibling != entt::null) {
            auto& next_sibling = registry.get<Node>(node.nextSibling);
            next_sibling.prevSibling = entt::null;
        }

        // set the parent's first child to sibling1 (node's next sibling)
        if (!node.IsRoot())
            registry.get<Node>(node.parent).firstChild = node.nextSibling;
    }
    // any other child case, example: | sibling1 | node | sibling2 | , 
    // remove the node by setting sibling1's next sibling to sibling2 and
    // settings sibling2's previous sibling to sibling1
    else {
        auto& prev_sibling = registry.get<Node>(node.prevSibling);
        prev_sibling.nextSibling = node.nextSibling;

        if (node.nextSibling != entt::null) {
            auto& next_sibling = registry.get<Node>(node.nextSibling);
            next_sibling.prevSibling = node.prevSibling;
        }
    }

    // nullifying parent and first child detaches it from the node hierarchy.
    // also, nextSibling is used by CollapseTransforms to loop to the next child
    node.parent = entt::null;
    node.firstChild = entt::null;
}


void CollapseTransforms(entt::registry& registry, Node& node, entt::entity entity) {
    // Remove will destroy the node but we still need to recurse to all its children,
    // so store it beforehand!
    const auto first_child = node.firstChild;
    
    if (!registry.all_of<Mesh>(entity)) {
        auto& transform = registry.get<Transform>(entity);

        for (auto it = node.firstChild; it != entt::null; it = registry.get<Node>(it).nextSibling) {
            auto& child_transform = registry.get<Transform>(it);
            child_transform.localTransform *= transform.localTransform;
            child_transform.Decompose();
        }

        NodeSystem::sRemove(registry, node);
    }

    for (auto it = first_child; it != entt::null; it = registry.get<Node>(it).nextSibling) {
        auto& child = registry.get<Node>(it);
        CollapseTransforms(registry, child, it);
    }
}


void NodeSystem::sOptimize(entt::registry& registry) {
    // collect all the root nodes we can recurse from
    std::vector<entt::entity> root_entities;
    
    for (auto& [entity, node] : registry.view<Node>().each())
        if (node.IsRoot() && node.firstChild != entt::null)
            root_entities.push_back(entity);

    for (const auto& entity : root_entities)
        CollapseTransforms(registry, registry.get<Node>(entity), entity);

    for (auto& [entity, node] : registry.view<Node>().each())
        if (!node.IsConnected() && !registry.all_of<Mesh>(entity))
            registry.destroy(entity);
}


std::vector<entt::entity> NodeSystem::sGetFlatHierarchy(entt::registry& registry, Node& startingNode) {
    std::vector<entt::entity> result;
    std::queue<entt::entity> entities;

    entities.push(entt::to_entity(registry, startingNode));

    while (!entities.empty()) {
        auto& current = registry.get<Node>(entities.front());
        result.push_back(entities.front());
        entities.pop();

        if (current.HasChildren())
            for (auto it = current.firstChild; it != entt::null; it = registry.get<Node>(it).nextSibling)
                entities.push(it);
    }

    // reverse so iteration starts at leaves
    std::reverse(result.begin(), result.end());

    // remove the original node entity
    result.pop_back();

    return result;
}

} // namespace Raekor