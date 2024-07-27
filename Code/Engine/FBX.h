#pragma once

#include "scene.h"

namespace RK {

class FBXImporter : public Importer
{
public:
	FBXImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer) {}
	bool LoadFromFile(const std::string& inFile, Assets* inAssets) override;

private:
	void ParseNode(const ufbx_node* inNode, Entity inParent, Entity inEntity);

	void ConvertMesh(Entity inEntity, const ufbx_mesh* inMesh, const ufbx_mesh_material& inMaterial);
	void ConvertBones(Entity inEntity, const ufbx_bone_list* inSkeleton);
	void ConvertMaterial(Entity inEntity, const ufbx_material* inMaterial);

private:
	Path m_Directory;
	ufbx_scene* m_FbxScene;
	std::vector<Entity> m_Materials;
	std::vector<Entity> m_CreatedNodeEntities;
};

}