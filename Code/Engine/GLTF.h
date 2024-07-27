#pragma once

#include "scene.h"

namespace RK {

class Async;
class Assets;

class Mesh;
class Material;
class Skeleton;

class GltfImporter : public Importer
{
public:
	GltfImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer) {}
	~GltfImporter();

	bool LoadFromFile(const std::string& inFile, Assets* inAssets) override;

private:
	void ParseNode(const cgltf_node& gltfNode, Entity parent, glm::mat4 transform);

	bool ConvertMesh(Entity inEntity, const cgltf_primitive& inMesh);
	void ConvertLight(Entity inEntity, const cgltf_light& inLight);
	void ConvertBones(Entity inEntity, const cgltf_node& inSkeleton);
	void ConvertMaterial(Entity inEntity, const cgltf_material& inMaterial);
	void ConvertAnimation(Entity inEntity, const cgltf_animation& inAnimation);

	Mat4x4 GetLocalTransform(const cgltf_node& inNode);
	int GetJointIndex(const cgltf_node* inSkinNode, const cgltf_node* inJointNode);

private:
	Path m_Directory;
	cgltf_data* m_GltfData = nullptr;
	Array<Entity> m_Materials;
	Array<Entity> m_Animations;
	Array<Entity> m_CreatedNodeEntities;
};

} // raekor
