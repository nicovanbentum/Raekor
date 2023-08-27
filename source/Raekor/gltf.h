#pragma once

#include "scene.h"

namespace Raekor {

class Async;
class Assets;

class Mesh;
class Material;
class Skeleton;

class GltfImporter {
public:
	GltfImporter(Scene& inScene, IRenderer* inRenderer) : m_Scene(inScene), m_Renderer(inRenderer) {}
	~GltfImporter();

	bool LoadFromFile(const std::string& inFile, Assets* inAssets);

private:
	void ParseNode(const cgltf_node& gltfNode, Entity parent, glm::mat4 transform);

	void ConvertMesh(Entity inEntity, const cgltf_primitive& inAssimpMesh);
	void ConvertBones(Entity inEntity, const cgltf_node& inAssimpMesh);
	void ConvertMaterial(Entity inEntity, const cgltf_material& inAssimpMaterial);

	int GetJointIndex(const cgltf_node* inSkinNode, const cgltf_node* inJointNode);

private:
	Scene& m_Scene;
	IRenderer* m_Renderer = nullptr;

	Path m_Directory;
	cgltf_data* m_GltfData = nullptr;
	std::vector<Entity> m_Materials;
	std::vector<int> m_MaterialRefs;
	std::vector<Entity> m_CreatedNodeEntities;
};

} // raekor
