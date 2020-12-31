#pragma once

#include "pch.h"
#include "components.h"

namespace Raekor {

class NodeSystem {
public:
    static void append(entt::registry& registry, ecs::NodeComponent& parent, ecs::NodeComponent& child);
    
    static void remove(entt::registry& registry, ecs::NodeComponent& node);

    static std::vector<entt::entity> getTree(entt::registry& registry, ecs::NodeComponent& node);
};

}