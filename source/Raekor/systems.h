#pragma once

#include "pch.h"
#include "components.h"

namespace Raekor {

class NodeSystem {
public:
    static void sAppend(entt::registry& registry, Node& parent, Node& child);
    static void sRemove(entt::registry& registry, Node& node);
    static void sCollapseTransforms(entt::registry& registry, Node& node, entt::entity entity);

    static std::vector<entt::entity> sGetFlatHierarchy(entt::registry& registry, Node& node);
};

}