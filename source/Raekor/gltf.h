#pragma once

namespace Raekor {

class Scene;
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
	void parseMaterial(cgltf_material& gltfMaterial, entt::entity entity);
	void parseNode(const cgltf_node& gltfNode, entt::entity entity, entt::entity parent);
	void parseMeshes(const cgltf_node& gltfNode, entt::entity entity, entt::entity parent);

	void LoadMesh(entt::entity entity, const cgltf_mesh& assimpMesh);
	void LoadBones(entt::entity entity, const cgltf_node& assimpMesh);
	void LoadMaterial(entt::entity entity, const cgltf_material& assimpMaterial);


private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
	std::function<void(Skeleton& skeleton, Mesh& mesh)> m_UploadSkeletonCallback = nullptr;

	Scene& m_Scene;
	fs::path m_Directory;
	cgltf_data* m_GltfData = nullptr;
	std::vector<entt::entity> m_Materials;
	std::vector<entt::entity> m_CreatedNodes;
};

} // raekor
