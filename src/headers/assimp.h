#pragma once

namespace Assimp {

glm::mat4 toMat4(const aiMatrix4x4& from);

} // assimp

namespace Raekor {

class Scene;
class Async;
class Assets;

class AssimpImporter {
public:
	AssimpImporter(Scene& scene) : scene(scene) {}
	bool LoadFromFile(Async& async, Assets& assets, const std::string& file);

private:
	void parseMaterial(aiMaterial* assimpMaterial, entt::entity entity);
	void parseNode(const aiNode* assimpNode, entt::entity entity, entt::entity parent);
	void parseMeshes(const aiNode* assimpNode, entt::entity entity, entt::entity parent);

	void LoadMesh(entt::entity entity, const aiMesh* assimpMesh);
	void LoadBones(entt::entity entity, const aiMesh* assimpMesh);
	void LoadMaterial(entt::entity entity, const aiMaterial* assimpMaterial);

private:
	Scene& scene;
	const aiScene* assimpScene;
	std::filesystem::path directory;
	std::vector<entt::entity> materials;
};

} // raekor
