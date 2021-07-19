#pragma once

#include "pch.h"
#include "components.h"

namespace Raekor {

class NodeSystem {
public:
    static void append(entt::registry& registry, Node& parent, Node& child);
    
    static void remove(entt::registry& registry, Node& node);

    static std::vector<entt::entity> getFlatHierarchy(entt::registry& registry, Node& node);
};

}