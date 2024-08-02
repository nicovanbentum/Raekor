#include "PCH.h"
#include "OBJ.h"
#include "Profiler.h"
#include "Components.h"
#include "Application.h"

namespace RK {

bool OBJImporter::LoadFromFile(const String& inFile, Assets* inAssets)
{
	PROFILE_FUNCTION_CPU();

	Timer timer;

	m_Directory = Path(inFile).parent_path() / "";

	std::fstream file(inFile, std::ios::in);
	if (!file.is_open())
		return false;

	String mtl_file = "";
	String name = Path(inFile).filename().string();

	OBJMesh obj_mesh;
	bool usemtl = false;

	String buffer;
	while (std::getline(file, buffer))
	{
		std::istringstream line(buffer);
		std::string token;
		line >> token;

		if (token == "o")
		{
			line >> name;
		}
		else if (token == "v")
		{
			Vec3 position;
			line >> position.x >> position.y >> position.z;
			obj_mesh.positions.push_back(position);
		}
		else if (token == "vt")
		{
			Vec2 uv;
			line >> uv.x >> uv.y;
			obj_mesh.texcoords.push_back(uv);
		}
		else if (token == "vn")
		{
			Vec3 normal;
			line >> normal.x >> normal.y >> normal.z;
			obj_mesh.normals.push_back(normal);
		}
		else if (token == "f")
		{
			uint8_t vertex_count = 0;
			StaticArray<Vec3, 4> vertices;

			String face;
			while (line >> face)
			{
				std::replace(face.begin(), face.end(), '/', ' ');
				std::istringstream spaced_face(face);

				int v_index = 0, vt_index = 0, vn_index = 0;
				spaced_face >> v_index >> vt_index >> vn_index;

				if (v_index < 0) v_index = obj_mesh.positions.size() + v_index + 1;
				if (vn_index < 0) vn_index = obj_mesh.normals.size() + vn_index + 1;
				if (vt_index < 0) vt_index = obj_mesh.texcoords.size() + vt_index + 1;

				assert(v_index >= 0 && v_index <= obj_mesh.positions.size());
				assert(vn_index >= 0 && vn_index <= obj_mesh.normals.size());
				assert(vt_index >= 0 && vt_index <= obj_mesh.texcoords.size());

				vertices[vertex_count++] = Vec3(v_index, vt_index, vn_index);

				// found a quad
				if (vertex_count == 4)
				{
					obj_mesh.v_indices.push_back(vertices[0].x);
					obj_mesh.vt_indices.push_back(vertices[0].y);
					obj_mesh.vn_indices.push_back(vertices[0].z);

					obj_mesh.v_indices.push_back(vertices[2].x);
					obj_mesh.vt_indices.push_back(vertices[2].y);
					obj_mesh.vn_indices.push_back(vertices[2].z);
				}

				obj_mesh.v_indices.push_back(v_index);
				obj_mesh.vt_indices.push_back(vt_index);
				obj_mesh.vn_indices.push_back(vn_index);
			}
		}
		else if (token == "usemtl")
		{

		}
		else if (token == "mtllib")
		{
			if (mtl_file.empty())
				line >> mtl_file;
			else
				std::cout << std::format("[OBJ] Multiple material libraries found, using {}\n", mtl_file);
		}
	}

	if (!mtl_file.empty())
		LoadMaterials(m_Directory / mtl_file);

	if (inAssets != nullptr)
		m_Scene.LoadMaterialTextures(*inAssets, m_Materials);

	Entity entity = m_Scene.CreateSpatialEntity(name);
	Mesh& mesh = m_Scene.Add<Mesh>(entity);

	mesh.uvs.reserve(obj_mesh.texcoords.size());
	mesh.normals.reserve(obj_mesh.normals.size());
	mesh.positions.reserve(obj_mesh.positions.size());

	uint32_t index_count = obj_mesh.v_indices.size();

	for (uint32_t i = 0; i < index_count; i++)
	{
		uint32_t v_index = obj_mesh.v_indices[i] - 1;
		uint32_t vt_index = obj_mesh.vt_indices[i] - 1;
		uint32_t vn_index = obj_mesh.vn_indices[i] - 1;

		if (!obj_mesh.texcoords.empty())
			mesh.uvs.push_back(obj_mesh.texcoords[vt_index]);
		
		if (!obj_mesh.normals.empty())
			mesh.normals.push_back(obj_mesh.normals[vn_index]);
		
		if (!obj_mesh.positions.empty())
			mesh.positions.push_back(obj_mesh.positions[v_index]);

		mesh.indices.push_back(i);
	}

	mesh.CalculateBoundingBox();

	if (mesh.normals.empty() && !mesh.positions.empty())
		mesh.CalculateNormals();

	if (mesh.tangents.empty() && !mesh.uvs.empty())
		mesh.CalculateTangents();

	if (mesh.vertices.empty())
		mesh.CalculateVertices();

	std::cout << std::format("[OBJ] Importer took {:.1f} ms.\n", Timer::sToMilliseconds(timer.GetElapsedTime()));

	if (m_Renderer)
		m_Renderer->UploadMeshBuffers(entity, mesh);

	return true;
}


bool OBJImporter::LoadMaterials(const Path& inFile)
{
	std::fstream file(inFile, std::ios::in);
	if (!file.is_open())
		return false;

	bool newmtl = false;
	OBJMaterial material;

	String buffer;
	while (std::getline(file, buffer))
	{
		std::istringstream line(buffer);
		std::string token;
		line >> token;

		if (token == "Kd")
		{
			line >> material.diffuse.r >> material.diffuse.g >> material.diffuse.b;
		}
		else if (token == "d")
		{
			line >> material.alpha;
		}
		else if (token == "Tr")
		{
			line >> material.alpha; material.alpha = 1.0 - material.alpha;
		}
		else if (token == "Ks")
		{
			line >> material.specular.r >> material.specular.g >> material.specular.b;
		}
		else if (token == "Ns")
		{
			line >> material.specularExp;
		}
		else if (token == "Ka")
		{
			line >> material.ambient.r >> material.ambient.g >> material.ambient.b;
		}
		else if (token == "Ni")
		{
			line >> material.ior;
		}
		else if (token == "Tf")
		{
			line >> material.alphaColor.r >> material.alphaColor.g >> material.alphaColor.b;
		}
		else if (token == "illum")
		{
			line >> material.illum;
		}
		else if (token == "Ke")
		{
			line >> material.emissive.r >> material.emissive.g >> material.emissive.b;
		}
		else if (token == "Pr")
		{
			line >> material.roughness;
		}
		else if (token == "Pm")
		{
			line >> material.metallic;
		}
		else if (token == "Ps")
		{
			line >> material.sheen;
		}
		else if (token == "Pc")
		{
			line >> material.ccThickness;
		}
		else if (token == "Pcr")
		{
			line >> material.ccRoughness;
		}
		else if (token == "aniso")
		{
			line >> material.anisotropy;
		}
		else if (token == "anisor")
		{
			line >> material.anisoRotation;
		}
		else if (token == "map_bump" || token == "bump")
		{
			line >> material.bumpMap; // TODO: parse out options
		}
		else if (token == "map_d")
		{
			line >> material.alphaMap; // TODO: parse out options
		}
		else if (token == "decal")
		{
			line >> material.decalMap; // TODO: parse out options
		}
		else if (token == "map_Ka")
		{
			line >> material.ambientMap; // TODO: parse out options
		}
		else if (token == "map_Kd")
		{
			line >> material.diffuseMap; // TODO: parse out options
		}
		else if (token == "map_Ks")
		{
			line >> material.specularMap; // TODO: parse out options
		}
		else if (token == "map_Ns")
		{
			line >> material.specularExpMap; // TODO: parse out options
		}
		else if (token == "disp")
		{
			line >> material.displacementMap; // TODO: parse out options
		}
		else if (token == "map_Ps")
		{
			line >> material.sheenMap; // TODO: parse out options
		}
		else if (token == "norm")
		{
			line >> material.normalMap; // TODO: parse out options
		}
		else if (token == "map_Pm")
		{
			line >> material.metallicMap; // TODO: parse out options
		}
		else if (token == "map_Ke")
		{
			line >> material.emissiveMap; // TODO: parse out options
		}
		else if (token == "map_Pr")
		{
			line >> material.roughnessMap; // TODO: parse out options
		}
		else if (token == "map_RMA")
		{
			line >> material.rmaMap; // TODO: parse out options
		}
		else if (token == "map_ORM")
		{
			line >> material.ormMap; // TODO: parse out options
		}
		else if (token == "newmtl")
		{
			if (newmtl)
				m_ObjMaterials.push_back(material);

			material = OBJMaterial();
			line >> material.name;
			newmtl = true;
		}
	}

	for (const OBJMaterial& material : m_ObjMaterials)
	{
		Entity entity = m_Scene.Create();
		ConvertMaterial(entity, material);
		m_Materials.push_back(entity);
	}

	return true;
}


void OBJImporter::ConvertMaterial(Entity inEntity, const OBJMaterial& inMaterial)
{
	Name& name = m_Scene.Add<Name>(inEntity, Name(inMaterial.name));
	Material& material = m_Scene.Add<Material>(inEntity);

	material.albedo = Vec4(inMaterial.diffuse, inMaterial.alpha);
	material.emissive = inMaterial.emissive;
	material.metallic = inMaterial.metallic;
	material.roughness = inMaterial.roughness;
	material.isTransparent = inMaterial.alpha < 1.0;

	if (!inMaterial.diffuseMap.empty())
		material.albedoFile = TextureAsset::GetCachedPath(m_Directory.string() + inMaterial.diffuseMap);
	
	if (!inMaterial.normalMap.empty())
		material.normalFile = TextureAsset::GetCachedPath(m_Directory.string() + inMaterial.normalMap);

	if (!inMaterial.emissiveMap.empty())
		material.emissiveFile = TextureAsset::GetCachedPath(m_Directory.string() + inMaterial.emissiveMap);

	if (!inMaterial.metallicMap.empty())
		material.metallicFile = TextureAsset::GetCachedPath(m_Directory.string() + inMaterial.metallicMap);

	if (!inMaterial.roughnessMap.empty())
		material.roughnessFile = TextureAsset::GetCachedPath(m_Directory.string() + inMaterial.roughnessMap);
}

} // RK