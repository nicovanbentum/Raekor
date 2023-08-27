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
typedef uint32_t Entity;

class AssimpImporter {
public:
	AssimpImporter(Scene& inScene, IRenderer* inRenderer) : m_Scene(inScene), m_Renderer(inRenderer) {}
	bool LoadFromFile(const std::string& inFile, Assets* inAssets);

	const Path& GetDirectory() { return m_Directory; }
	const aiScene* GetAiScene() { return m_AiScene; }
	const std::vector<Entity>& GetMaterials() { return m_Materials; }

private:
	void ParseMaterial(aiMaterial* inAssimpMaterial, Entity inEntity);
	void ParseNode(const aiNode* inAssimpNode, Entity inEntity, Entity inParent);
	void ParseMeshes(const aiNode* inAssimpNode, Entity inEntity, Entity inParent);

	void LoadMesh(Entity inEntity, const aiMesh* inAssimpMesh);
	void LoadBones(Entity inEntity, const aiMesh* inAssimpMesh);
	void LoadMaterial(Entity inEntity, const aiMaterial* inAssimpMaterial);

private:
	Scene& m_Scene;
	IRenderer* m_Renderer = nullptr;

	Path m_Directory;
	const aiScene* m_AiScene = nullptr;
	std::shared_ptr<Assimp::Importer> m_Importer;
	std::vector<Entity> m_Materials;
};

} // raekor
