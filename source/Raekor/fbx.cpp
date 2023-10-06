#include "pch.h"
#include "fbx.h"
#include "timer.h"
#include "systems.h"
#include "components.h"
#include "application.h"

namespace Raekor {

bool FBXImporter::LoadFromFile(const std::string& inFile, Assets* inAssets)
{
	/*
	* LOAD FBX FROM DISK
	*/
	Timer timer;

	m_Directory = Path(inFile).parent_path() / "";

	auto opts = ufbx_load_opts {};
	opts.allow_null_material = true;
	opts.allow_nodes_out_of_root = true;
	auto error = ufbx_error {};

	m_FbxScene = ufbx_load_file(inFile.c_str(), &opts, &error);
	if (!m_FbxScene)
		return false;

	std::cout << "[FBX Import] File load took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	for (const auto& material : m_FbxScene->materials)
	{
		auto entity = m_Scene.Create();
		auto& nameComponent = m_Scene.Add<Name>(entity);

		if (material->name.length && strcmp(material->name.data, "") != 0)
			nameComponent.name = material->name.data;
		else
			nameComponent.name = "Material " + std::to_string(uint32_t(entity));

		ConvertMaterial(entity, material);

		m_Materials.push_back(entity);
	}

	for (const auto& node : m_FbxScene->nodes)
	{
		if (node->is_root)
			ParseNode(node, NULL_ENTITY, glm::mat4(1.0f));
	}

	const auto root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());
	auto& root_node = m_Scene.Get<Node>(root_entity);

	for (const auto& entity : m_CreatedNodeEntities)
	{
		auto& node = m_Scene.Get<Node>(entity);

		if (node.IsRoot())
			NodeSystem::sAppend(m_Scene, root_entity, root_node, entity, node);
	}

	std::cout << "[GLTF Import] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
		m_Scene.LoadMaterialTextures(*inAssets, Slice(m_Materials.data(), m_Materials.size()));

	ufbx_free_scene(m_FbxScene);

	return false;
}


void FBXImporter::ParseNode(const ufbx_node* inNode, Entity inParent, glm::mat4 inTransform)
{
	auto local_transform = glm::mat4(1.0f);

	const auto& translation = inNode->local_transform.translation;
	local_transform = glm::translate(local_transform, Vec3(translation.x, translation.y, translation.z));

	const auto& rotation = inNode->local_transform.rotation;
	local_transform = local_transform * glm::toMat4(Quat(rotation.w, rotation.x, rotation.y, rotation.z));

	const auto& scale = inNode->local_transform.scale;
	local_transform = glm::scale(local_transform, Vec3(scale.x, scale.y, scale.z));

	// Calculate global transform
	inTransform *= local_transform;

	if (inNode->mesh)
	{
		auto entity = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity());

		if (inNode->name.length)
			m_Scene.Get<Name>(entity).name = inNode->name.data;
		else if (inNode->mesh && inNode->mesh->name.length)
			m_Scene.Get<Name>(entity).name = inNode->mesh->name.data;
		else
			m_Scene.Get<Name>(entity).name = "Mesh " + std::to_string(uint32_t(entity));

		auto& mesh_transform = m_Scene.Get<Transform>(entity);
		mesh_transform.localTransform = inTransform;
		mesh_transform.Decompose();

		// set the new entity's parent
		if (inParent != NULL_ENTITY)
			NodeSystem::sAppend(m_Scene, inParent, m_Scene.Get<Node>(inParent), entity, m_Scene.Get<Node>(entity));

		if (inNode->mesh->materials.count == 1)
		{
			ConvertMesh(entity, inNode->mesh, inNode->mesh->materials[0]);
		}
		else if (inNode->mesh->materials.count > 1)
		{
			for (const auto& [index, material] : gEnumerate(inNode->mesh->materials))
			{
				auto clone = m_CreatedNodeEntities.emplace_back(m_Scene.Clone(entity));
				ConvertMesh(clone, inNode->mesh, material);

				auto& name = m_Scene.Get<Name>(clone);
				name.name += "-" + std::to_string(index);

				// multiple mesh entities of the same node all get parented to that node's transform, so their local transform can just be identity
				auto& transform = m_Scene.Get<Transform>(clone);
				transform.localTransform = glm::mat4(1.0f);
				transform.Decompose();

				NodeSystem::sAppend(m_Scene, entity, m_Scene.Get<Node>(entity), clone, m_Scene.Get<Node>(clone));
			}
		}

		// TODO: ANIMATION
		/*if (inNode->bone)
			ConvertBones(entity, inNode->bone);*/
	}

	for (const auto& child : inNode->children)
		ParseNode(child, inParent, inTransform);
}


void FBXImporter::ConvertMesh(Entity inEntity, const ufbx_mesh* inMesh, const ufbx_mesh_material& inMaterial)
{
	auto& mesh = m_Scene.Add<Mesh>(inEntity);

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
	const auto num_tri_indices = inMesh->max_face_triangles * 3;
	auto tri_indices = std::vector<uint32_t>(num_tri_indices);

	// First fetch all vertices into a flat non-indexed buffer, we also need to
	// triangulate the faces
	size_t num_indices = 0;

	for (size_t fi = 0; fi < inMaterial.num_faces; fi++)
	{
		ufbx_face face = inMesh->faces.data[inMaterial.face_indices.data[fi]];
		size_t num_tris = ufbx_triangulate_face(tri_indices.data(), num_tri_indices, inMesh, face);

		// Iterate through every vertex of every triangle in the triangulated result
		for (auto vi = 0; vi < num_tris * 3; vi++)
		{
			const auto ix = tri_indices[vi];

			const auto pos = ufbx_get_vertex_vec3(&inMesh->vertex_position, ix);
			mesh.positions.push_back(Vec3(pos.x, pos.y, pos.z));

			const auto uv = inMesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&inMesh->vertex_uv, ix) : ufbx_vec2 { 0 };
			mesh.uvs.push_back(Vec2(uv.x, uv.y));

			if (inMesh->vertex_normal.exists)
			{
				const auto normal = ufbx_get_vertex_vec3(&inMesh->vertex_normal, ix);
				mesh.normals.push_back(glm::normalize(Vec3(normal.x, normal.y, normal.z)));
			}

			if (inMesh->vertex_tangent.exists)
			{
				const auto tangent = ufbx_get_vertex_vec3(&inMesh->vertex_tangent, ix);
				mesh.tangents.push_back(glm::normalize(Vec3(tangent.x, tangent.y, tangent.z)));
			}

			num_indices++;
		}
	}

	std::vector<ufbx_vertex_stream> streams;
	auto& pos_stream = streams.emplace_back();
	pos_stream.data = mesh.positions.data();
	pos_stream.vertex_size = sizeof(Vec3);

	auto& uv_stream = streams.emplace_back();
	uv_stream.data = mesh.uvs.data();
	uv_stream.vertex_size = sizeof(Vec2);

	if (inMesh->vertex_normal.exists)
	{
		auto& normal_stream = streams.emplace_back();
		normal_stream.data = mesh.normals.data();
		normal_stream.vertex_size = sizeof(Vec3);
	}

	if (inMesh->vertex_tangent.exists)
	{
		auto& tangent_stream = streams.emplace_back();
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
	auto& material = m_Scene.Add<Material>(inEntity);

	const auto& albedo = inMaterial->pbr.base_color;
	if (albedo.has_value)
		for (uint32_t i = 0; i < albedo.value_components; i++)
			material.albedo[i] = albedo.value_vec4.v[i];

	if (albedo.texture && albedo.texture_enabled && albedo.texture->type == UFBX_TEXTURE_FILE)
		material.albedoFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + albedo.texture->relative_filename.data);

	const auto& normal_map = inMaterial->pbr.normal_map;
	if (normal_map.texture && normal_map.texture_enabled && normal_map.texture->type == UFBX_TEXTURE_FILE)
		material.normalFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + normal_map.texture->relative_filename.data);

	const auto& roughness = inMaterial->pbr.roughness;
	if (roughness.texture && roughness.texture_enabled && roughness.texture->type == UFBX_TEXTURE_FILE)
		material.roughnessFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + roughness.texture->relative_filename.data);

	const auto& metallic = inMaterial->pbr.metalness;
	if (metallic.texture && metallic.texture_enabled && metallic.texture->type == UFBX_TEXTURE_FILE)
		material.metallicFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + metallic.texture->relative_filename.data);

	const auto& emission = inMaterial->pbr.emission_color;
	if (emission.has_value)
		for (uint32_t i = 0; i < emission.value_components; i++)
			material.emissive[i] = emission.value_vec4.v[i];

	if (emission.texture && emission.texture_enabled && emission.texture->type == UFBX_TEXTURE_FILE)
		material.emissiveFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + emission.texture->relative_filename.data);

	material.metallic = inMaterial->pbr.metalness.has_value ? inMaterial->pbr.metalness.value_real : material.metallic;
	material.roughness = inMaterial->pbr.roughness.has_value ? inMaterial->pbr.roughness.value_real : material.roughness;
}

} // namespace::Raekor