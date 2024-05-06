#include "pch.h"
#include "fbx.h"
#include "iter.h"
#include "timer.h"
#include "components.h"
#include "application.h"

namespace RK {

bool FBXImporter::LoadFromFile(const std::string& inFile, Assets* inAssets)
{
	/*
	* LOAD FBX FROM DISK
	*/
	Timer timer;

	m_Directory = Path(inFile).parent_path() / "";

	ufbx_load_opts opts = ufbx_load_opts {};
	opts.allow_null_material = true;
	opts.allow_nodes_out_of_root = true;

	ufbx_error error = ufbx_error {};
	m_FbxScene = ufbx_load_file(inFile.c_str(), &opts, &error);
	
	if (m_FbxScene == nullptr)
		return false;

	std::cout << "[FBX Import] File load took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	for (const ufbx_material* material : m_FbxScene->materials)
	{
		Entity entity = m_Scene.Create();
		Name& name = m_Scene.Add<Name>(entity);

		if (material->name.length && strcmp(material->name.data, "") != 0)
			name.name = material->name.data;
		else
			name.name = "Material " + std::to_string(uint32_t(entity));

		ConvertMaterial(entity, material);

		m_Materials.push_back(entity);
	}

	for (const ufbx_node* node : m_FbxScene->nodes)
	{
		if (node->is_root)
		{
			Entity root = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity(node->name.data));
			ParseNode(node, m_Scene.GetRootEntity(), root);
		}
	}

	const Entity root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());

	for (Entity entity : m_CreatedNodeEntities)
	{
		if (!m_Scene.HasParent(entity) || m_Scene.GetParent(entity) == m_Scene.GetRootEntity())
			m_Scene.ParentTo(entity, root_entity);
	}

	std::cout << "[GLTF] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
		m_Scene.LoadMaterialTextures(*inAssets, Slice(m_Materials.data(), m_Materials.size()));

	ufbx_free_scene(m_FbxScene);

	return false;
}


void FBXImporter::ParseNode(const ufbx_node* inNode, Entity inParent, Entity inEntity)
{
	if (inNode->name.length)
		m_Scene.Get<Name>(inEntity).name = inNode->name.data;
	else if (inNode->mesh && inNode->mesh->name.length)
		m_Scene.Get<Name>(inEntity).name = inNode->mesh->name.data;
	else
		m_Scene.Get<Name>(inEntity).name = inNode->mesh ? "Mesh " : "Entity " + std::to_string(uint32_t(inEntity));

	if (inParent != Entity::Null)
		m_Scene.ParentTo(inEntity, inParent);

	Mat4x4 local_transform = Mat4x4(1.0f);

	const ufbx_vec3& translation = inNode->local_transform.translation;
	local_transform = glm::translate(local_transform, Vec3(translation.x, translation.y, translation.z));

	const ufbx_quat& rotation = inNode->local_transform.rotation;
	local_transform = local_transform * glm::toMat4(Quat(rotation.w, rotation.x, rotation.y, rotation.z));

	const ufbx_vec3& scale = inNode->local_transform.scale;
	local_transform = glm::scale(local_transform, Vec3(scale.x, scale.y, scale.z));

	Transform& transform = m_Scene.Get<Transform>(inEntity);
	transform.localTransform = local_transform;
	transform.Decompose();
	
	if (inNode->mesh)
	{
		if (inNode->mesh->materials.count == 1)
		{
			ConvertMesh(inEntity, inNode->mesh, inNode->mesh->materials[0]);
		}
		else if (inNode->mesh->materials.count > 1)
		{
			for (const auto& [index, material] : gEnumerate(inNode->mesh->materials))
			{
				Entity sub_entity = m_Scene.CreateSpatialEntity();
				ConvertMesh(sub_entity, inNode->mesh, material);

				Name& sub_name = m_Scene.Get<Name>(sub_entity);
				sub_name.name = m_Scene.Get<Name>(inEntity).name + "-" + std::to_string(index);

				m_Scene.ParentTo(sub_entity, inEntity);
				m_CreatedNodeEntities.emplace_back(sub_entity);
			}
		}

		// TODO: ANIMATION
		//if (inNode->bone)
		//	ConvertBones(inEntity, inNode->bone);
	}

	for (const ufbx_node* child : inNode->children)
	{
		Entity child_entity = m_Scene.CreateSpatialEntity(child->name.data);
		ParseNode(child, inEntity, child_entity);
	}
}


void FBXImporter::ConvertMesh(Entity inEntity, const ufbx_mesh* inMesh, const ufbx_mesh_material& inMaterial)
{
	Mesh& mesh = m_Scene.Add<Mesh>(inEntity);

	if (!inMesh->num_indices || !inMaterial.num_triangles)
		return;

	mesh.uvs.reserve(inMesh->vertex_uv.indices.count);
	mesh.normals.reserve(inMesh->vertex_normal.indices.count);
	mesh.tangents.reserve(inMesh->vertex_tangent.indices.count);
	mesh.positions.reserve(inMesh->vertex_position.indices.count);

	for (int i = 0; i < m_FbxScene->materials.count; i++)
	{
		if (inMaterial.material == m_FbxScene->materials[i])
			mesh.material = m_Materials[i];
	}

	// Temporary buffers
	int num_tri_indices = inMesh->max_face_triangles * 3;
	Array<uint32_t> tri_indices(num_tri_indices);

	// First fetch all vertices into a flat non-indexed buffer, we also need to
	// triangulate the faces
	size_t num_indices = 0;

	for (size_t fi = 0; fi < inMaterial.num_faces; fi++)
	{
		ufbx_face face = inMesh->faces.data[inMaterial.face_indices.data[fi]];
		size_t num_tris = ufbx_triangulate_face(tri_indices.data(), num_tri_indices, inMesh, face);

		// Iterate through every vertex of every triangle in the triangulated result
		for (int vi = 0; vi < num_tris * 3; vi++)
		{
			const uint32_t ix = tri_indices[vi];

			const ufbx_vec3 pos = ufbx_get_vertex_vec3(&inMesh->vertex_position, ix);
			mesh.positions.push_back(Vec3(pos.x, pos.y, pos.z));

			const ufbx_vec2 uv = inMesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&inMesh->vertex_uv, ix) : ufbx_vec2 { 0 };
			mesh.uvs.push_back(Vec2(uv.x, uv.y));

			if (inMesh->vertex_normal.exists)
			{
				const ufbx_vec3 normal = ufbx_get_vertex_vec3(&inMesh->vertex_normal, ix);
				mesh.normals.push_back(glm::normalize(Vec3(normal.x, normal.y, normal.z)));
			}

			if (inMesh->vertex_tangent.exists)
			{
				const ufbx_vec3 tangent = ufbx_get_vertex_vec3(&inMesh->vertex_tangent, ix);
				mesh.tangents.push_back(glm::normalize(Vec3(tangent.x, tangent.y, tangent.z)));
			}

			num_indices++;
		}
	}

	std::vector<ufbx_vertex_stream> streams;

	ufbx_vertex_stream& pos_stream = streams.emplace_back();
	pos_stream.data = mesh.positions.data();
	pos_stream.vertex_size = sizeof(Vec3);

	ufbx_vertex_stream& uv_stream = streams.emplace_back();
	uv_stream.data = mesh.uvs.data();
	uv_stream.vertex_size = sizeof(Vec2);

	if (inMesh->vertex_normal.exists)
	{
		ufbx_vertex_stream& normal_stream = streams.emplace_back();
		normal_stream.data = mesh.normals.data();
		normal_stream.vertex_size = sizeof(Vec3);
	}

	if (inMesh->vertex_tangent.exists)
	{
		ufbx_vertex_stream& tangent_stream = streams.emplace_back();
		tangent_stream.data = mesh.tangents.data();
		tangent_stream.vertex_size = sizeof(Vec3);
	}

	// Optimize the flat vertex buffer into an indexed one. `ufbx_generate_indices()`
	// compacts the vertex buffer and returns the number of used vertices.
	ufbx_error error;
	mesh.indices.resize(num_indices);
	size_t num_vertices = ufbx_generate_indices(streams.data(), streams.size(), mesh.indices.data(), num_indices, NULL, &error);
	assert(error.type == UFBX_ERROR_NONE);

	mesh.CalculateAABB();

	if (mesh.normals.empty() && !mesh.positions.empty())
		mesh.CalculateNormals();

	if (mesh.tangents.empty() && !mesh.uvs.empty())
		mesh.CalculateTangents();

	if (m_Renderer)
		m_Renderer->UploadMeshBuffers(inEntity, mesh);
}


void FBXImporter::ConvertBones(Entity inEntity, const ufbx_bone_list* inSkeleton)
{

}


void FBXImporter::ConvertMaterial(Entity inEntity, const ufbx_material* inMaterial)
{
	Material& material = m_Scene.Add<Material>(inEntity);

	const ufbx_material_map& albedo = inMaterial->pbr.base_color;
	if (albedo.has_value)
		for (uint32_t i = 0; i < albedo.value_components; i++)
			material.albedo[i] = albedo.value_vec4.v[i];

	if (albedo.texture && albedo.texture_enabled && albedo.texture->type == UFBX_TEXTURE_FILE)
		material.albedoFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + albedo.texture->relative_filename.data);

	const ufbx_material_map& normal_map = inMaterial->pbr.normal_map;
	if (normal_map.texture && normal_map.texture_enabled && normal_map.texture->type == UFBX_TEXTURE_FILE)
		material.normalFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + normal_map.texture->relative_filename.data);

	const ufbx_material_map& roughness = inMaterial->pbr.roughness;
	if (roughness.texture && roughness.texture_enabled && roughness.texture->type == UFBX_TEXTURE_FILE)
		material.roughnessFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + roughness.texture->relative_filename.data);

	const ufbx_material_map& metallic = inMaterial->pbr.metalness;
	if (metallic.texture && metallic.texture_enabled && metallic.texture->type == UFBX_TEXTURE_FILE)
		material.metallicFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + metallic.texture->relative_filename.data);

	const ufbx_material_map& emission = inMaterial->pbr.emission_color;
	if (emission.has_value)
		for (uint32_t i = 0; i < emission.value_components; i++)
			material.emissive[i] = emission.value_vec4.v[i];

	if (emission.texture && emission.texture_enabled && emission.texture->type == UFBX_TEXTURE_FILE)
		material.emissiveFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + emission.texture->relative_filename.data);

	material.metallic = inMaterial->pbr.metalness.has_value ? inMaterial->pbr.metalness.value_real : material.metallic;
	material.roughness = inMaterial->pbr.roughness.has_value ? inMaterial->pbr.roughness.value_real : material.roughness;
}

} // namespace::Raekor