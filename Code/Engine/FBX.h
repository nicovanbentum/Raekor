#pragma once

#include "scene.h"

namespace RK {

struct MeshGeometry;

class FBXImporter : public Importer
{
public:
	FBXImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer) {}
	bool LoadFromFile(const String& inFile, Assets* inAssets) override;

private:
	void ParseNode(const ufbx_node* inNode, Entity inParent, Entity inEntity);

	void ConvertMeshes();
	void ConvertMesh(MeshGeometry& ioGeometry, const ufbx_mesh* inMesh, const ufbx_mesh_material& inMaterial);
	void AddMesh(Entity inEntity, const ufbx_mesh* inMesh, uint32_t inPart);
	void ConvertLight(Entity inEntity, const ufbx_light* inLight);
	void ConvertBones(Entity inEntity, const ufbx_bone_list* inSkeleton);
	void ConvertMaterial(Entity inEntity, const ufbx_material* inMaterial);

private:
	Path m_Directory;
	ufbx_scene* m_FbxScene;
	Array<Entity> m_Materials;
	Array<uint32_t> m_MeshOffsets;
	Array<Entity> m_CreatedNodeEntities;
};

}