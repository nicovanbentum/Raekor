#pragma once

#include "Scene.h"
#include "CVars.h"

namespace RK {

struct OBJMesh
{
	Array<uint32_t> v_indices;   // Token: f x//
	Array<uint32_t> vt_indices;  // Token: f /x/
	Array<uint32_t> vn_indices;  // Token: f //x

	void Clear() { v_indices.clear(); vt_indices.clear(); vn_indices.clear(); }
	bool IsEmpty() const { return v_indices.empty() && vt_indices.empty() && vn_indices.empty(); }
};

struct OBJMaterial
{
	// Blinn-Phong properties
	Vec3 diffuse = Vec3(0);		// Token: 'Kd'
	float alpha = 1.0;			// Token: 'd' or 'Tr'
	Vec3 specular = Vec3(0);	// Token: 'Ks'
	float specularExp = 1.0;	// Token: 'Ns'
	Vec3 ambient = Vec3(0);		// Token: 'Ka'
	float ior = 1.0;			// Token: 'Ni'
	Vec3 alphaColor = Vec3(0);  // Token: 'Tf'
	uint32_t illum = 0;			// Token: 'illum'

	// PBR properties
	Vec3 emissive = Vec3(0);	// Token: 'Ke'
	float roughness = 1.0;		// Token: 'Pr'
	float metallic = 0.0;		// Token: 'Pm'
	float sheen = 0.0;			// Token: 'Ps'
	float ccThickness = 0.0;	// Token: 'Pc'
	float ccRoughness = 0.0;	// Token: 'Pcr'
	float anisotropy = 0.0;		// Token: 'aniso'
	float anisoRotation = 0.0;	// Token: 'anisor'

	// Material Name
	String name;
	
	// Blinn-Phong textures
	String bumpMap;				// Token: 'map_bump' or 'bump'
	String alphaMap;			// Token: 'map_d'
	String decalMap;			// Token: 'decal'
	String ambientMap;			// Token: 'map_Ka'
	String diffuseMap;			// Token: 'map_Kd'
	String specularMap;			// Token: 'map_Ks'
	String specularExpMap;		// Token: 'map_Ns'
	String displacementMap;		// Token: 'disp'

	// PBR Textures
	String sheenMap;			// Token: 'map_Ps'
	String normalMap;			// Token: 'norm'
	String metallicMap;			// Token: 'map_Pm'
	String emissiveMap;			// Token: 'map_Ke'
	String roughnessMap;		// Token: 'map_Pr'
	
	// Extra PBR Textures
	String rmaMap;				// Token: 'map_RMA'
	String ormMap;				// Token: 'map_ORM'
};

class OBJImporter : public Importer
{
public:
	struct 
	{
		int& importGrouping = g_CVariables->Create("obj_import_groupings", 1, true);
		int& importMaterials = g_CVariables->Create("obj_import_materials", 1, true);
	} m_Settings;

public:
	OBJImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer) {}
	bool LoadFromFile(const String& inFile, Assets* inAssets) override;

private:
	bool LoadMaterials(const Path& inFilePath);
	void ConvertMesh(Entity inEntity, const OBJMesh& inMesh);
	void ConvertMaterial(Entity inEntity, const OBJMaterial& inMaterial);

private:
	Path m_Directory;
	Array<Vec3> m_Normals;		
	Array<Vec3> m_Positions;		 
	Array<Vec2> m_Texcoords;
	Array<Entity> m_Meshes;
	Array<Entity> m_Materials;
	Array<OBJMaterial> m_ObjMaterials;
};

}