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
	GltfImporter(Scene& inScene) : m_Scene(inScene) {}
	~GltfImporter();

	bool LoadFromFile(Assets& inAssets, const std::string& inFile);

	template<typename Fn> void SetUploadMeshCallbackFunction(Fn&& inFunction) { m_UploadMeshCallback = inFunction; }
	template<typename Fn> void SetUploadMaterialCallbackFunction(Fn&& inFunction) { m_UploadMaterialCallback = inFunction; }
	template<typename Fn> void SetUploadSkeletonCallbackFunction(Fn&& inFunction) { m_UploadSkeletonCallback = inFunction; }

private:
	void ParseNode(const cgltf_node& gltfNode, entt::entity parent, glm::mat4 transform);

	void ConvertMesh(Entity inEntity, const cgltf_primitive& inAssimpMesh);
	void ConvertBones(Entity inEntity, const cgltf_node& inAssimpMesh);
	void ConvertMaterial(Entity inEntity, const cgltf_material& inAssimpMaterial);

	int GetJointIndex(const cgltf_node* inSkinNode, const cgltf_node* inJointNode);

private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
	std::function<void(Skeleton& skeleton, Mesh& mesh)> m_UploadSkeletonCallback = nullptr;

	Scene& m_Scene;
	Path m_Directory;
	cgltf_data* m_GltfData = nullptr;
	std::vector<entt::entity> m_Materials;
	std::vector<entt::entity> m_CreatedNodeEntities;
};

} // raekor
