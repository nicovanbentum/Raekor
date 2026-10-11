#include "pch.h"
#include "fbx.h"
#include "iter.h"
#include "timer.h"
#include "threading.h"
#include "components.h"
#include "application.h"

namespace RK {

bool FBXImporter::LoadFromFile(const String& inFile, Assets* inAssets)
{
	/*
	* LOAD FBX FROM DISK
	*/
	Timer timer;

	m_Directory = Path(inFile).parent_path() / "";

	ufbx_load_opts opts = ufbx_load_opts {};
    opts.target_unit_meters = 1.0f;
	opts.allow_null_material = true;
	opts.allow_nodes_out_of_root = true;
    opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
    opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
    opts.target_axes.front = UFBX_COORDINATE_AXIS_POSITIVE_Z;

	ufbx_error error = ufbx_error {};
	m_FbxScene = ufbx_load_file(inFile.c_str(), &opts, &error);
	
	if (m_FbxScene == nullptr)
		return false;

	gLogInfo("FBX", "File load took {:.2f} ms", Timer::sToMilliseconds(timer.Restart()));

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

	CreateMeshAsset(inFile);
	ConvertMeshes();

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

	FinishMeshAsset(inAssets);

	gLogInfo("FBX", "Meshes & nodes took {:.2f} ms", Timer::sToMilliseconds(timer.Restart()));

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
		m_Scene.LoadMaterialTextures(*inAssets);

	ufbx_free_scene(m_FbxScene);

	return true;
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
		String name = m_Scene.Has<Name>(inEntity) ? m_Scene.Get<Name>(inEntity).name : "Mesh";

		if (inNode->mesh->materials.count == 1)
		{
			AddMesh(inEntity, inNode->mesh, 0);
		}
		else if (inNode->mesh->materials.count > 1)
		{
			for (uint32_t index = 0; index < inNode->mesh->materials.count; index++)
			{
				Entity sub_entity = m_Scene.CreateSpatialEntity();
				AddMesh(sub_entity, inNode->mesh, index);

				Name& sub_name = m_Scene.Get<Name>(sub_entity);
				sub_name.name = name + "-" + std::to_string(index);

				m_Scene.ParentTo(sub_entity, inEntity);
				m_CreatedNodeEntities.emplace_back(sub_entity);
			}
		}

		// TODO: ANIMATION
		//if (inNode->bone)
		//	ConvertBones(inEntity, inNode->bone);
	}

	if (inNode->light)
	{
		if (inNode->light->type == UFBX_LIGHT_POINT || inNode->light->type == UFBX_LIGHT_SPOT)
			ConvertLight(inEntity, inNode->light);
	}

	for (const ufbx_node* child : inNode->children)
	{
		Entity child_entity = m_Scene.CreateSpatialEntity(child->name.data);
		ParseNode(child, inEntity, child_entity);
	}
}


void FBXImporter::ConvertMeshes()
{
	uint32_t mesh_count = 0;
	m_MeshOffsets.resize(m_FbxScene->meshes.count);

	for (const auto& [index, fbx_mesh] : gEnumerate(m_FbxScene->meshes))
	{
		m_MeshOffsets[index] = mesh_count;
		mesh_count += uint32_t(fbx_mesh->materials.count);
	}

	Array<MeshGeometry>& meshes = m_MeshAsset->GetMeshes();
	meshes.resize(mesh_count);

	g_JobSystem.ParallelFor(uint32_t(m_FbxScene->meshes.count), 1, [&](uint32_t inIndex)
	{
		const ufbx_mesh* fbx_mesh = m_FbxScene->meshes[inIndex];

		for (uint32_t part = 0; part < fbx_mesh->materials.count; part++)
		{
			MeshGeometry& geometry = meshes[m_MeshOffsets[inIndex] + part];

			const String name = fbx_mesh->name.length ? String(fbx_mesh->name.data) : std::format("Mesh {}", inIndex);
			geometry.name = fbx_mesh->materials.count > 1 ? std::format("{}-{}", name, part) : name;

			ConvertMesh(geometry, fbx_mesh, fbx_mesh->materials[part]);
		}
	});
}


void FBXImporter::AddMesh(Entity inEntity, const ufbx_mesh* inMesh, uint32_t inPart)
{
	const uint32_t mesh_index = m_MeshOffsets[inMesh->typed_id] + inPart;

	if (m_MeshAsset->GetMesh(mesh_index)->IsEmpty())
		return;

	Entity material = Entity::Null;

	for (int i = 0; i < m_FbxScene->materials.count; i++)
	{
		if (inMesh->materials[inPart].material == m_FbxScene->materials[i])
			material = m_Materials[i];
	}

	Importer::AddMesh(inEntity, mesh_index, material);
}


void FBXImporter::ConvertMesh(MeshGeometry& ioGeometry, const ufbx_mesh* inMesh, const ufbx_mesh_material& inMaterial)
{
	if (!inMesh->num_indices || !inMaterial.num_triangles)
		return;

	ioGeometry.uvs.reserve(inMesh->vertex_uv.indices.count);
	ioGeometry.normals.reserve(inMesh->vertex_normal.indices.count);
	ioGeometry.tangents.reserve(inMesh->vertex_tangent.indices.count);
	ioGeometry.positions.reserve(inMesh->vertex_position.indices.count);

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
			ioGeometry.positions.push_back(Vec3(pos.x, pos.y, pos.z));

			const ufbx_vec2 uv = inMesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&inMesh->vertex_uv, ix) : ufbx_vec2 { 0 };
			ioGeometry.uvs.push_back(Vec2(uv.x, uv.y));

			if (inMesh->vertex_normal.exists)
			{
				const ufbx_vec3 normal = ufbx_get_vertex_vec3(&inMesh->vertex_normal, ix);
				ioGeometry.normals.push_back(glm::normalize(Vec3(normal.x, normal.y, normal.z)));
			}

			if (inMesh->vertex_tangent.exists)
			{
				const ufbx_vec3 tangent = ufbx_get_vertex_vec3(&inMesh->vertex_tangent, ix);
				ioGeometry.tangents.push_back(glm::normalize(Vec3(tangent.x, tangent.y, tangent.z)));
			}

			num_indices++;
		}
	}

	std::vector<ufbx_vertex_stream> streams;

	ufbx_vertex_stream& pos_stream = streams.emplace_back();
	pos_stream.data = ioGeometry.positions.data();
	pos_stream.vertex_size = sizeof(Vec3);

	ufbx_vertex_stream& uv_stream = streams.emplace_back();
	uv_stream.data = ioGeometry.uvs.data();
	uv_stream.vertex_size = sizeof(Vec2);

	if (inMesh->vertex_normal.exists)
	{
		ufbx_vertex_stream& normal_stream = streams.emplace_back();
		normal_stream.data = ioGeometry.normals.data();
		normal_stream.vertex_size = sizeof(Vec3);
	}

	if (inMesh->vertex_tangent.exists)
	{
		ufbx_vertex_stream& tangent_stream = streams.emplace_back();
		tangent_stream.data = ioGeometry.tangents.data();
		tangent_stream.vertex_size = sizeof(Vec3);
	}

	// Optimize the flat vertex buffer into an indexed one. `ufbx_generate_indices()`
	// compacts the vertex buffer and returns the number of used vertices.
	ufbx_error error;
	ioGeometry.indices.resize(num_indices);
	size_t num_vertices = ufbx_generate_indices(streams.data(), streams.size(), ioGeometry.indices.data(), num_indices, NULL, &error);
	assert(error.type == UFBX_ERROR_NONE);

	for (Array<Vec3>* stream : { &ioGeometry.positions, &ioGeometry.normals, &ioGeometry.tangents })
	{
		if (!stream->empty())
			stream->resize(num_vertices);
	}

	ioGeometry.uvs.resize(num_vertices);

	ioGeometry.CalculateBoundingBox();

	if (ioGeometry.normals.empty() && !ioGeometry.positions.empty())
		ioGeometry.CalculateNormals();

	if (ioGeometry.tangents.empty() && !ioGeometry.uvs.empty())
		ioGeometry.CalculateTangents();

	if (ioGeometry.vertices.empty())
		ioGeometry.CalculateVertices();
}


void FBXImporter::ConvertLight(Entity inEntity, const ufbx_light* inLight)
{
	Light& light = m_Scene.Add<Light>(inEntity);

	switch (inLight->type)
	{
		case UFBX_LIGHT_POINT:
			light.type = LIGHT_TYPE_POINT;
			break;
		case UFBX_LIGHT_SPOT:
			light.type = LIGHT_TYPE_SPOT;
			break;
		default:
			RK_ASSERT(false);
	};
	
	light.color = Vec4(inLight->color.x, inLight->color.y, inLight->color.z, inLight->intensity);
	light.attributes.x = inLight->outer_angle;
	light.attributes.y = inLight->inner_angle;
	light.attributes.z = inLight->inner_angle;
	light.attributes.w = inLight->outer_angle;
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
	{
		material.albedoFile = TextureAsset::GetCachedPath(m_Directory.string() + albedo.texture->relative_filename.data);

		if (albedo.texture->layers.count == 1)
		{
			if (albedo.texture->layers.data->alpha < 1.0f || albedo.texture->layers.data->blend_mode == UFBX_BLEND_REPLACE)
				material.blendMode = MATERIAL_BLEND_MODE_MASKED;
		}
	}

	const ufbx_material_map& normal_map = inMaterial->pbr.normal_map;
	if (normal_map.texture && normal_map.texture_enabled && normal_map.texture->type == UFBX_TEXTURE_FILE)
		material.normalFile = TextureAsset::GetCachedPath(m_Directory.string() + normal_map.texture->relative_filename.data);

	const ufbx_material_map& roughness = inMaterial->pbr.roughness;
	if (roughness.texture && roughness.texture_enabled && roughness.texture->type == UFBX_TEXTURE_FILE)
		material.roughnessFile = TextureAsset::GetCachedPath(m_Directory.string() + roughness.texture->relative_filename.data);

	const ufbx_material_map& metallic = inMaterial->pbr.metalness;
	if (metallic.texture && metallic.texture_enabled && metallic.texture->type == UFBX_TEXTURE_FILE)
		material.metallicFile = TextureAsset::GetCachedPath(m_Directory.string() + metallic.texture->relative_filename.data);

	// good chance we only have specular maps, try those...
	if (material.metallicFile.empty() && material.roughnessFile.empty())
	{
		const ufbx_material_map& specular_color = inMaterial->pbr.specular_color;
		if (specular_color.texture && specular_color.texture_enabled && specular_color.texture->type == UFBX_TEXTURE_FILE)
		{
			String specular_color_filepath = TextureAsset::GetCachedPath(m_Directory.string() + specular_color.texture->relative_filename.data);
			material.metallicFile = specular_color_filepath;
			material.roughnessFile = specular_color_filepath;

			material.metallic = 1.0f;
			material.roughness = 1.0f;
		}
	}

	const ufbx_material_map& emission_color = inMaterial->pbr.emission_color;
	if (emission_color.has_value)
	{
		for (uint32_t i = 0; i < emission_color.value_components; i++)
			material.emissive[i] = emission_color.value_vec4.v[i];
	}
	else
	{
		const ufbx_material_map& emission_factor = inMaterial->pbr.emission_factor;
		if (emission_factor.has_value)
		{
			for (uint32_t i = 0; i < emission_factor.value_components; i++)
				material.emissive[i] = emission_factor.value_vec4.v[i];
		}
	}

	if (emission_color.texture && emission_color.texture_enabled && emission_color.texture->type == UFBX_TEXTURE_FILE)
		material.emissiveFile = TextureAsset::GetCachedPath(m_Directory.string() + emission_color.texture->relative_filename.data);

	material.metallic = inMaterial->pbr.metalness.has_value ? inMaterial->pbr.metalness.value_real : material.metallic;
	material.roughness = inMaterial->pbr.roughness.has_value ? inMaterial->pbr.roughness.value_real : material.roughness;
}

} // namespace::Raekor