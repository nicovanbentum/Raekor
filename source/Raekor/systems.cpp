#include "pch.h"
#include "systems.h"
#include "components.h"
#include "application.h"

namespace Raekor {

void NodeSystem::sAppend(ECStorage& inECS, Entity parentEntity, Node& parent, Entity childEntity, Node& child)
{
	child.parent = parentEntity;

	// if its the parent's first child we simply assign it
	if (parent.firstChild == Entity::Null)
	{
		parent.firstChild = childEntity;
		return;
	}
	else
	{
		// ensure we can get the first child's node
		auto last_child = parent.firstChild;
		assert(inECS.Get<Node>(last_child).prevSibling == Entity::Null);

		// traverse to the end of the sibling chain
		while (inECS.Get<Node>(last_child).nextSibling != Entity::Null)
			last_child = inECS.Get<Node>(last_child).nextSibling;

		// update siblings
		auto& last_node = inECS.Get<Node>(last_child);
		last_node.nextSibling = childEntity;
		child.prevSibling = last_child;
	}
}


void NodeSystem::sRemove(ECStorage& inECS, Node& node)
{
	for (auto it = node.firstChild; it != Entity::Null; it = inECS.Get<Node>(it).nextSibling)
	{
		auto& child = inECS.Get<Node>(it);
		// this will either hook it up to the node's parent or set it to null, 
		// node's parent is the desired outcome when collapsing transforms
		child.parent = node.parent;
	}

	// handle first child case, example: node | sibling1 | sibling2 | etc.
	if (node.prevSibling == Entity::Null)
	{
		// set sibling1's previous sibling to null (since it is now the first child)
		if (node.nextSibling != Entity::Null)
		{
			auto& next_sibling = inECS.Get<Node>(node.nextSibling);
			next_sibling.prevSibling = Entity::Null;
		}

		// set the parent's first child to sibling1 (node's next sibling)
		if (!node.IsRoot())
			inECS.Get<Node>(node.parent).firstChild = node.nextSibling;
	}
	// any other child case, example: | sibling1 | node | sibling2 | , 
	// remove the node by setting sibling1's next sibling to sibling2 and
	// settings sibling2's previous sibling to sibling1
	else
	{
		auto& prev_sibling = inECS.Get<Node>(node.prevSibling);
		prev_sibling.nextSibling = node.nextSibling;

		if (node.nextSibling != Entity::Null)
		{
			auto& next_sibling = inECS.Get<Node>(node.nextSibling);
			next_sibling.prevSibling = node.prevSibling;
		}
	}

	// nullifying parent and first child detaches it from the node hierarchy.
	// also, nextSibling is used by CollapseTransforms to loop to the next child
	node.parent = Entity::Null;
	node.firstChild = Entity::Null;
}


void NodeSystem::sCollapseTransforms(ECStorage& inECS, Node& node, Entity inEntity)
{
	// Remove will destroy the node but we still need to recurse to all its children,
	// so store it beforehand!
	const auto first_child = node.firstChild;

	if (!inECS.Has<Mesh>(inEntity))
	{
		auto& transform = inECS.Get<Transform>(inEntity);

		for (auto it = node.firstChild; it != Entity::Null; it = inECS.Get<Node>(it).nextSibling)
		{
			auto& child_transform = inECS.Get<Transform>(it);
			child_transform.localTransform *= transform.worldTransform;
			child_transform.Decompose();
		}

		NodeSystem::sRemove(inECS, node);
	}

	for (auto it = first_child; it != Entity::Null; it = inECS.Get<Node>(it).nextSibling)
	{
		auto& child = inECS.Get<Node>(it);
		sCollapseTransforms(inECS, child, it);
	}
}


std::vector<Entity> NodeSystem::sGetFlatHierarchy(ECStorage& inECS, Entity inEntity)
{
	if (!inECS.Has<Node>(inEntity))
		return {};

	std::vector<Entity> result;
	std::queue<Entity> entities;

	entities.push(inEntity);

	while (!entities.empty())
	{
		auto& current = inECS.Get<Node>(entities.front());
		result.push_back(entities.front());
		entities.pop();

		if (current.HasChildren())
			for (auto it = current.firstChild; it != Entity::Null; it = inECS.Get<Node>(it).nextSibling)
				entities.push(it);
	}

	// reverse so iteration starts at leaves
	std::reverse(result.begin(), result.end());

	// remove the original node entity
	result.pop_back();

	return result;
}


void IRenderInterface::UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets)
{
	assert(Material::Default.IsLoaded() && "Default material not loaded, did the programmer forget to initialize its gpu maps before opening a scene?");

	auto UploadTexture = [&](const std::string& inFile, bool inIsSRGB, uint8_t inSwizzle, uint32_t inDefaultMap, uint32_t& ioGpuMap) 
	{
		if (auto asset = inAssets.GetAsset<TextureAsset>(inFile))
			ioGpuMap = UploadTextureFromAsset(asset, inIsSRGB, inSwizzle);
		else
			ioGpuMap = inDefaultMap;
	};

	UploadTexture(inMaterial.albedoFile,	true,  inMaterial.gpuAlbedoMapSwizzle,	  Material::Default.gpuAlbedoMap,	 inMaterial.gpuAlbedoMap);
	UploadTexture(inMaterial.normalFile,	false, inMaterial.gpuNormalMapSwizzle,	  Material::Default.gpuNormalMap,	 inMaterial.gpuNormalMap);
	UploadTexture(inMaterial.emissiveFile,	false, inMaterial.gpuEmissiveMapSwizzle,  Material::Default.gpuEmissiveMap,  inMaterial.gpuEmissiveMap);
	UploadTexture(inMaterial.metallicFile,	false, inMaterial.gpuMetallicMapSwizzle,  Material::Default.gpuMetallicMap,  inMaterial.gpuMetallicMap);
	UploadTexture(inMaterial.roughnessFile, false, inMaterial.gpuRoughnessMapSwizzle, Material::Default.gpuRoughnessMap, inMaterial.gpuRoughnessMap);
}

} // namespace Raekor