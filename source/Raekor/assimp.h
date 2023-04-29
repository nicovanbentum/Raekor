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
	AssimpImporter(Scene& inScene) : m_Scene(inScene) {}
	bool LoadFromFile(Assets& inAssets, const std::string& inFile);

	template<typename Fn> void SetUploadMeshCallbackFunction(Fn&& inFunction)	  { m_UploadMeshCallback = inFunction; }
	template<typename Fn> void SetUploadMaterialCallbackFunction(Fn&& inFunction) { m_UploadMaterialCallback = inFunction; }
	template<typename Fn> void SetUploadSkeletonCallbackFunction(Fn&& inFunction) { m_UploadSkeletonCallback = inFunction; }

private:
	void ParseMaterial(aiMaterial* inAssimpMaterial, entt::entity inEntity);
	void ParseNode(const aiNode* inAssimpNode, entt::entity inEntity, entt::entity inParent);
	void ParseMeshes(const aiNode* inAssimpNode, entt::entity inEntity, entt::entity inParent);

	void LoadMesh(entt::entity inEntity, const aiMesh* inAssimpMesh);
	void LoadBones(entt::entity inEntity, const aiMesh* inAssimpMesh);
	void LoadMaterial(entt::entity inEntity, const aiMaterial* inAssimpMaterial);


private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
	std::function<void(Skeleton& skeleton, Mesh& mesh)> m_UploadSkeletonCallback = nullptr;

	Scene& m_Scene;
	Path m_Directory;
	const aiScene* m_AiScene;
	std::vector<entt::entity> m_Materials;
};

} // raekor
