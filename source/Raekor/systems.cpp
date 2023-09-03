#include "pch.h"
#include "systems.h"
#include "components.h"
#include "application.h"

namespace Raekor {

void NodeSystem::sAppend(ecs::ECS& inECS, Entity parentEntity, Node& parent, Entity childEntity, Node& child) {
    child.parent = parentEntity;

    // if its the parent's first child we simply assign it
    if (parent.firstChild == NULL_ENTITY) {
        parent.firstChild = childEntity;
        return;
    } else {
        // ensure we can get the first child's node
        auto last_child  = parent.firstChild;
        assert(inECS.Get<Node>(last_child).prevSibling == NULL_ENTITY);

        // traverse to the end of the sibling chain
         while (inECS.Get<Node>(last_child).nextSibling != NULL_ENTITY)
            last_child = inECS.Get<Node>(last_child).nextSibling;

        // update siblings
        auto& last_node = inECS.Get<Node>(last_child);
        last_node.nextSibling = childEntity;
        child.prevSibling = last_child;
    }
}


void NodeSystem::sRemove(ecs::ECS& inECS, Node& node) {
    for (auto it = node.firstChild; it != NULL_ENTITY; it = inECS.Get<Node>(it).nextSibling) {
        auto& child = inECS.Get<Node>(it);
        // this will either hook it up to the node's parent or set it to null, 
        // node's parent is the desired outcome when collapsing transforms
        child.parent = node.parent;
    }

    // handle first child case, example: node | sibling1 | sibling2 | etc.
    if (node.prevSibling == NULL_ENTITY) {
        // set sibling1's previous sibling to null (since it is now the first child)
        if (node.nextSibling != NULL_ENTITY) {
            auto& next_sibling = inECS.Get<Node>(node.nextSibling);
            next_sibling.prevSibling = NULL_ENTITY;
        }

        // set the parent's first child to sibling1 (node's next sibling)
        if (!node.IsRoot())
            inECS.Get<Node>(node.parent).firstChild = node.nextSibling;
    }
    // any other child case, example: | sibling1 | node | sibling2 | , 
    // remove the node by setting sibling1's next sibling to sibling2 and
    // settings sibling2's previous sibling to sibling1
    else {
        auto& prev_sibling = inECS.Get<Node>(node.prevSibling);
        prev_sibling.nextSibling = node.nextSibling;

        if (node.nextSibling != NULL_ENTITY) {
            auto& next_sibling = inECS.Get<Node>(node.nextSibling);
            next_sibling.prevSibling = node.prevSibling;
        }
    }

    // nullifying parent and first child detaches it from the node hierarchy.
    // also, nextSibling is used by CollapseTransforms to loop to the next child
    node.parent = NULL_ENTITY;
    node.firstChild = NULL_ENTITY;
}


void NodeSystem::sCollapseTransforms(ecs::ECS& inECS, Node& node, Entity inEntity) {
    // Remove will destroy the node but we still need to recurse to all its children,
    // so store it beforehand!
    const auto first_child = node.firstChild;
    
    if (!inECS.Has<Mesh>(inEntity)) {
        auto& transform = inECS.Get<Transform>(inEntity);

        for (auto it = node.firstChild; it != NULL_ENTITY; it = inECS.Get<Node>(it).nextSibling) {
            auto& child_transform = inECS.Get<Transform>(it);
            child_transform.localTransform *= transform.worldTransform;
            child_transform.Decompose();
        }

        NodeSystem::sRemove(inECS, node);
    }

    for (auto it = first_child; it != NULL_ENTITY; it = inECS.Get<Node>(it).nextSibling) {
        auto& child = inECS.Get<Node>(it);
        sCollapseTransforms(inECS, child, it);
    }
}


std::vector<Entity> NodeSystem::sGetFlatHierarchy(ecs::ECS& inECS, Entity inEntity) {
    if (!inECS.Has<Node>(inEntity))
        return {};

    std::vector<Entity> result;
    std::queue<Entity> entities;

    entities.push(inEntity);

    while (!entities.empty()) {
        auto& current = inECS.Get<Node>(entities.front());
        result.push_back(entities.front());
        entities.pop();

        if (current.HasChildren())
            for (auto it = current.firstChild; it != NULL_ENTITY; it = inECS.Get<Node>(it).nextSibling)
                entities.push(it);
    }

    // reverse so iteration starts at leaves
    std::reverse(result.begin(), result.end());

    // remove the original node entity
    result.pop_back();

    return result;
}


void IRenderInterface::UploadMaterialTextures(Material& inMaterial, Assets& inAssets) {
    assert(Material::Default.IsLoaded() && "Default material not loaded, did you forget to initialize its gpu maps before opening a scene?");

    if (auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.albedoFile); asset)
        inMaterial.gpuAlbedoMap = UploadTextureFromAsset(asset, true);
    else
        inMaterial.gpuAlbedoMap = Material::Default.gpuAlbedoMap;

    if (auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.normalFile); asset)
        inMaterial.gpuNormalMap = UploadTextureFromAsset(asset);
    else
        inMaterial.gpuNormalMap = Material::Default.gpuNormalMap;

    if (auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.metalroughFile); asset)
        inMaterial.gpuMetallicRoughnessMap = UploadTextureFromAsset(asset);
    else
        inMaterial.gpuMetallicRoughnessMap = Material::Default.gpuMetallicRoughnessMap;

}

} // namespace Raekor