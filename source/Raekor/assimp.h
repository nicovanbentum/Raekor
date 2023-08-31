#pragma once

#ifndef DEPRECATE_ASSIMP

#include "scene.h"

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

class AssimpImporter : public Importer {
public:
	AssimpImporter(Scene& inScene, IRenderer* inRenderer) : Importer(inScene, inRenderer) {}
	bool LoadFromFile(const std::string& inFile, Assets* inAssets) override;

private:
	void ParseMaterial(aiMaterial* inAssimpMaterial, Entity inEntity);
	void ParseNode(const aiNode* inAssimpNode, Entity inEntity, Entity inParent);
	void ParseMeshes(const aiNode* inAssimpNode, Entity inEntity, Entity inParent);

	void LoadMesh(Entity inEntity, const aiMesh* inAssimpMesh);
	void LoadBones(Entity inEntity, const aiMesh* inAssimpMesh);
	void LoadMaterial(Entity inEntity, const aiMaterial* inAssimpMaterial);

private:
	Path m_Directory;
	const aiScene* m_AiScene = nullptr;
	std::shared_ptr<Assimp::Importer> m_Importer;
	std::vector<Entity> m_Materials;
};

} // raekor

#endif // DEPRECATE_ASSIMP
