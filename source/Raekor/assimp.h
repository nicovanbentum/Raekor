#pragma once

namespace Assimp {

glm::mat4 toMat4(const aiMatrix4x4& from);

} // assimp

namespace Raekor {

class Scene;
class Async;
class Assets;

class Mesh;
class Material;
class Skeleton;

class AssimpImporter {
public:
	AssimpImporter(Scene& scene) : m_Scene(scene) {}
	bool LoadFromFile(Assets& assets, const std::string& file);

	template<typename Fn> void SetUploadMeshCallbackFunction(Fn&& fn) { m_UploadMeshCallback = fn; }
	template<typename Fn> void SetUploadMaterialCallbackFunction(Fn&& fn) { m_UploadMaterialCallback = fn; }
	template<typename Fn> void SetUploadSkeletonCallbackFunction(Fn&& fn) { m_UploadSkeletonCallback = fn; }

private:
	void parseMaterial(aiMaterial* assimpMaterial, entt::entity entity);
	void parseNode(const aiNode* assimpNode, entt::entity entity, entt::entity parent);
	void parseMeshes(const aiNode* assimpNode, entt::entity entity, entt::entity parent);

	void LoadMesh(entt::entity entity, const aiMesh* assimpMesh);
	void LoadBones(entt::entity entity, const aiMesh* assimpMesh);
	void LoadMaterial(entt::entity entity, const aiMaterial* assimpMaterial);


private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
	std::function<void(Skeleton& skeleton, Mesh& mesh)> m_UploadSkeletonCallback = nullptr;

	Scene& m_Scene;
	fs::path m_Directory;
	const aiScene* m_AiScene;
	std::vector<entt::entity> m_Materials;
};

} // raekor
