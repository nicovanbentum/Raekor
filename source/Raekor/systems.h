#pragma once

#include "ecs.h"

namespace Raekor {

class ECStorage;
struct Node;

class NodeSystem
{
public:
	static void sAppend(ECStorage& registry, Entity parentEntity, Node& parent, Entity childEntity, Node& child);
	static void sRemove(ECStorage& registry, Node& node);
	static void sCollapseTransforms(ECStorage& registry, Node& node, Entity entity);

	static std::vector<Entity> sGetFlatHierarchy(ECStorage& registry, Entity inEntity);
};

}