#pragma once

#include "pch.h"
#include "components.h"
#include "application.h"

namespace Raekor {

class NodeSystem {
public:
    static void sAppend(entt::registry& registry, entt::entity parentEntity, Node& parent, entt::entity childEntity, Node& child);
    static void sRemove(entt::registry& registry, Node& node);
    static void sCollapseTransforms(entt::registry& registry, Node& node, entt::entity entity);

    static std::vector<entt::entity> sGetFlatHierarchy(entt::registry& registry, Node& node);
};

class RenderUtil {
public:
    static void sUploadMaterialTextures(IRenderer* inRenderer, Assets& inAssets, Material& inMaterial);
};

}