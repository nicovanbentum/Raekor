#pragma once

namespace Assimp {

glm::mat4 toMat4(const aiMatrix4x4& from);

} // assimp

namespace Raekor {

class Scene;
class Async;
class Assets;
class IRenderer;

class Mesh;
class Material;
class Skeleton;

class AssimpImporter {
public:
	AssimpImporter(Scene& inScene, IRenderer* inRenderer) : m_Scene(inScene), m_Renderer(inRenderer) {}
	bool LoadFromFile(Assets& inAssets, const std::string& inFile);

private:
	void ParseMaterial(aiMaterial* inAssimpMaterial, entt::entity inEntity);
	void ParseNode(const aiNode* inAssimpNode, entt::entity inEntity, entt::entity inParent);
	void ParseMeshes(const aiNode* inAssimpNode, entt::entity inEntity, entt::entity inParent);

	void LoadMesh(entt::entity inEntity, const aiMesh* inAssimpMesh);
	void LoadBones(entt::entity inEntity, const aiMesh* inAssimpMesh);
	void LoadMaterial(entt::entity inEntity, const aiMaterial* inAssimpMaterial);


private:
	Scene& m_Scene;
	IRenderer* m_Renderer = nullptr;

	Path m_Directory;
	const aiScene* m_AiScene = nullptr;
	std::vector<entt::entity> m_Materials;
};

} // raekor
