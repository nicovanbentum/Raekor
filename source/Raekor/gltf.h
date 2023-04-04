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
	GltfImporter(Scene& scene) : m_Scene(scene) {}
	~GltfImporter();

	bool LoadFromFile(Assets& assets, const std::string& file);

	template<typename Fn> void SetUploadMeshCallbackFunction(Fn&& fn) { m_UploadMeshCallback = fn; }
	template<typename Fn> void SetUploadMaterialCallbackFunction(Fn&& fn) { m_UploadMaterialCallback = fn; }
	template<typename Fn> void SetUploadSkeletonCallbackFunction(Fn&& fn) { m_UploadSkeletonCallback = fn; }

private:
	void ParseNode(const cgltf_node& gltfNode, entt::entity parent, glm::mat4 transform);

	void ConvertMesh(Entity inEntity, const cgltf_primitive& assimpMesh);
	void ConvertBones(Entity inEntity, const cgltf_node& assimpMesh);
	void ConvertMaterial(Entity inEntity, const cgltf_material& assimpMaterial);

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
