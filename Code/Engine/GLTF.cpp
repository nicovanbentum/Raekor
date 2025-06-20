#include "PCH.h"
#include "GLTF.h"
#include "Iter.h"
#include "Math.h"
#include "Scene.h"
#include "Timer.h"
#include "Threading.h"
#include "Components.h"
#include "Application.h"

namespace RK {

constexpr std::array cgltf_result_strings = 
{
	"cgltf_result_success",
	"cgltf_result_data_too_short",
	"cgltf_result_unknown_format",
	"cgltf_result_invalid_json",
	"cgltf_result_invalid_gltf",
	"cgltf_result_invalid_options",
	"cgltf_result_file_not_found",
	"cgltf_result_io_error",
	"cgltf_result_out_of_memory",
	"cgltf_result_legacy_gltf",
	"cgltf_result_max_enum"
};


bool handle_cgltf_error(cgltf_result result, const char* operation)
{
	if (result == cgltf_result_success)
		return true;

	std::cout << std::format("[GLTF] {} failed with value: {}", operation, cgltf_result_strings[result]);
	return false;
}


GltfImporter::~GltfImporter()
{
	if (m_GltfData)
		cgltf_free(m_GltfData);
}


bool GltfImporter::LoadFromFile(const String& inFile, Assets* inAssets)
{
	/*
	* LOAD GLTF FROM DISK
	*/
	Timer timer;

	m_Directory = Path(inFile).parent_path() / "";

	cgltf_options options = {};
	if (!handle_cgltf_error(cgltf_parse_file(&options, inFile.c_str(), &m_GltfData), "Parse"))
		return false;

	if (!handle_cgltf_error(cgltf_load_buffers(&options, m_GltfData, inFile.c_str()), "Load"))
		return false;

	if (!handle_cgltf_error(cgltf_validate(m_GltfData), "Validate"))
		return false;

	std::cout << "[GLTF] File load took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	/*
	* PARSE MATERIALS
	*/
	for (int index = 0; index < m_GltfData->materials_count; index++)
	{
		cgltf_material& gltf_material = m_GltfData->materials[index];

		Entity entity = m_Scene.Create();
		Name& nameComponent = m_Scene.Add<Name>(entity);

		if (gltf_material.name && strcmp(gltf_material.name, "") != 0)
			nameComponent.name = gltf_material.name;
		else
			nameComponent.name = "Material " + std::to_string(uint32_t(entity));

		ConvertMaterial(entity, gltf_material);

		m_Materials.push_back(entity);
	}

	/*
	* PARSE NODES & MESHES
	*/
	for (const cgltf_scene& scene : Slice(m_GltfData->scenes, m_GltfData->scenes_count))
		for (const cgltf_node* node : Slice(scene.nodes, scene.nodes_count))
			if (!node->parent)
				ParseNode(*node, m_Scene.GetRootEntity(), Mat4x4(1.0f));

	Entity root_entity = m_Scene.CreateSpatialEntity(Path(inFile).filename().string());

	for (Entity entity : m_CreatedNodeEntities)
	{
		if (!m_Scene.HasParent(entity) || m_Scene.GetParent(entity) == m_Scene.GetRootEntity())
			 m_Scene.ParentTo(entity, root_entity);
	}

	std::cout << "[GLTF] Meshes & nodes took " << Timer::sToMilliseconds(timer.Restart()) << " ms.\n";

	// Load the converted textures from disk and upload them to the GPU
	if (inAssets != nullptr)
		m_Scene.LoadMaterialTextures(*inAssets);

	/**/
	for (const cgltf_animation& gltf_animation : Slice(m_GltfData->animations, m_GltfData->animations_count))
	{
		Entity animation_entity = m_Scene.Create();

		Name& name = m_Scene.Add<Name>(animation_entity);
		Animation& animation = m_Scene.Add<Animation>(animation_entity, Animation(&gltf_animation));

		for (const cgltf_animation_channel& channel : Slice(gltf_animation.channels, gltf_animation.channels_count))
		{
			const String node_name = String(channel.target_node->name);

			animation.LoadKeyframes(node_name, &channel);

		}

		name.name = gltf_animation.name ? gltf_animation.name : "Animation";
		m_Animations.push_back(animation_entity);
	}

	for (Entity entity : m_Animations)
	{
		Animation& animation = m_Scene.Get<Animation>(entity);

		for (KeyFrames& keyframes : animation.GetKeyFrames())
		{
			const size_t max = glm::max(glm::max(keyframes.m_PositionKeys.size(), keyframes.m_RotationKeys.size()), keyframes.m_ScaleKeys.size());

			// if at least 1 of the 3 key channels has key data, the other 2 channels need at least 1 key
			if (max > 0)
			{
				if (keyframes.m_PositionKeys.empty())
					keyframes.m_PositionKeys.emplace_back(Vec3Key(0.0f, Vec3(0.0f, 0.0f, 0.0f)));

				if (keyframes.m_RotationKeys.empty())
					keyframes.m_RotationKeys.emplace_back(QuatKey(0.0f, Quat(1.0f, 0.0f, 0.0f, 0.0f)));

				if (keyframes.m_ScaleKeys.empty())
					keyframes.m_ScaleKeys.emplace_back(Vec3Key(0.0f, Vec3(1.0f, 1.0f, 1.0f)));
			}
		}
	}

	return true;
}


void GltfImporter::ParseNode(const cgltf_node& inNode, Entity inParent, glm::mat4 inTransform)
{
	bool has_transform = false;
	Mat4x4 local_transform(1.0f);

	if (inNode.has_matrix)
	{
		has_transform = true;
		static_assert( sizeof(inNode.matrix) == sizeof(local_transform) );
		memcpy(glm::value_ptr(local_transform), inNode.matrix, sizeof(inNode.matrix));
	}
	else
	{
		if (inNode.has_translation)
		{
			has_transform = true;
			local_transform = glm::translate(local_transform, Vec3(inNode.translation[0], inNode.translation[1], inNode.translation[2]));
		}

		if (inNode.has_rotation)
		{
			has_transform = true;
			local_transform = local_transform * glm::toMat4(glm::quat(inNode.rotation[3], inNode.rotation[0], inNode.rotation[1], inNode.rotation[2]));
		}

		if (inNode.has_scale)
		{
			has_transform = true;
			local_transform = glm::scale(local_transform, Vec3(inNode.scale[0], inNode.scale[1], inNode.scale[2]));
		}
	}

	// Calculate global transform
	inTransform *= local_transform;

	// initialize current entity to it's parent, so that if it doesn't have a transform it gets collapsed
	Entity entity = inParent;

	if (has_transform || (inNode.mesh && m_Scene.Has<Mesh>(inParent)))
	{
		String name = inNode.name ? inNode.name : "GLTF Node";
		entity = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity(name));

		// set the new entity's parent
		if (inParent != Entity::Null)
			m_Scene.ParentTo(entity, inParent);
	}

	if (has_transform)
	{
		Transform& mesh_transform = m_Scene.Get<Transform>(entity);
		mesh_transform.localTransform = local_transform;
		mesh_transform.Decompose();
	}
	
	if (inNode.mesh)
	{
		String name = m_Scene.Has<Name>(entity) ? m_Scene.Get<Name>(entity).name : "Mesh";

		if (inNode.mesh->primitives_count == 1)
		{
			ConvertMesh(entity, inNode.mesh->primitives[0]);
		}
		else if (inNode.mesh->primitives_count > 1)
		{
			for (const auto& [index, primitive] : gEnumerate(Slice(inNode.mesh->primitives, inNode.mesh->primitives_count)))
			{
				Entity sub_entity = m_Scene.CreateSpatialEntity();
				ConvertMesh(sub_entity, primitive);

				Name& sub_name = m_Scene.Get<Name>(sub_entity);
				sub_name.name = name + "-" + std::to_string(index);

				m_Scene.ParentTo(sub_entity, entity);
				m_CreatedNodeEntities.emplace_back(sub_entity);
			}
		}

		if (inNode.skin)
			ConvertBones(entity, inNode);
	}

	if (inNode.light)
		ConvertLight(entity, *inNode.light);

	for (cgltf_node* child : Slice(inNode.children, inNode.children_count))
		ParseNode(*child, entity, inTransform);
}


bool GltfImporter::ConvertMesh(Entity inEntity, const cgltf_primitive& inMesh)
{
	if (!inMesh.indices || inMesh.type != cgltf_primitive_type_triangles)
		return false;
	
	assert(!m_Scene.Has<Mesh>(inEntity));
	Mesh& mesh = m_Scene.Add<Mesh>(inEntity);

	for (int i = 0; i < m_GltfData->materials_count; i++)
	{
		if (inMesh.material == &m_GltfData->materials[i])
			mesh.material = m_Materials[i];
	}

	bool seen_uv0 = false;

	for (const cgltf_attribute& attribute : Slice(inMesh.attributes, inMesh.attributes_count))
	{
		const cgltf_size float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
		Array<float> accessor_data(float_count); // TODO: allocate this once?

		cgltf_float* data = accessor_data.data();
		cgltf_accessor_unpack_floats(attribute.data, data, float_count);
		const cgltf_size num_components = cgltf_num_components(attribute.data->type);

		// Skip any additional uv sets, we only support one set at the moment
		if (attribute.type == cgltf_attribute_type_texcoord && seen_uv0)
			continue;
		else if (attribute.type == cgltf_attribute_type_texcoord)
			seen_uv0 = true;

		for (uint32_t element_index = 0; element_index < attribute.data->count; element_index++)
		{
			switch (attribute.type)
			{
				case cgltf_attribute_type_position:
				{
					mesh.positions.emplace_back(data[0], data[1], data[2]);
				} break;
				case cgltf_attribute_type_texcoord:
				{
					mesh.uvs.emplace_back(data[0], data[1] * -1.0f);
				} break;
				case cgltf_attribute_type_normal:
				{
					mesh.normals.emplace_back(data[0], data[1], data[2]);
				} break;
				case cgltf_attribute_type_tangent:
				{
					mesh.tangents.emplace_back(data[0], data[1], data[2]);
				} break;
			}

			data += num_components;
		}
		// TODO: flip tangents
	}

	for (uint32_t i = 0; i < inMesh.indices->count; i++)
		mesh.indices.emplace_back(uint32_t(cgltf_accessor_read_index(inMesh.indices, i)));

	mesh.CalculateBoundingBox();

	if (mesh.normals.empty() && !mesh.positions.empty())
		mesh.CalculateNormals();

	if (mesh.tangents.empty() && !mesh.uvs.empty())
		mesh.CalculateTangents();

	if (mesh.vertices.empty())
		mesh.CalculateVertices();

	if (m_Renderer)
		m_Renderer->UploadMeshBuffers(inEntity, mesh);

	return true;
}



void GltfImporter::ConvertLight(Entity inEntity, const cgltf_light& inLight)
{
	if (inLight.name)
		m_Scene.Get<Name>(inEntity).name = inLight.name;
	else
		m_Scene.Get<Name>(inEntity).name = "Light " + std::to_string(inEntity);

	if (inLight.type == cgltf_light_type_point)
	{
		Light& light = m_Scene.Add<Light>(inEntity);

		light.type = LIGHT_TYPE_POINT;
		light.color = Vec4(inLight.color[0], inLight.color[1], inLight.color[2], inLight.intensity);
		light.attributes.x = inLight.range;
	}
	
	if (inLight.type == cgltf_light_type_spot)
	{
		Light& light = m_Scene.Add<Light>(inEntity);

		light.type = LIGHT_TYPE_SPOT;
		light.color = Vec4(inLight.color[0], inLight.color[1], inLight.color[2], inLight.intensity);
		light.attributes.x = inLight.range;
		light.attributes.y = inLight.spot_inner_cone_angle;
		light.attributes.z = inLight.spot_outer_cone_angle;
	}
}


Mat4x4 GltfImporter::GetLocalTransform(const cgltf_node& inNode)
{
	Mat4x4 local_transform = Mat4x4(1.0f);

	if (inNode.has_matrix)
	{
		static_assert( sizeof(inNode.matrix) == sizeof(local_transform) );
		memcpy(glm::value_ptr(local_transform), inNode.matrix, sizeof(inNode.matrix));
	}
	else
	{
		if (inNode.has_translation)
			local_transform = glm::translate(local_transform, Vec3(inNode.translation[0], inNode.translation[1], inNode.translation[2]));

		if (inNode.has_rotation)
			local_transform = local_transform * glm::toMat4(glm::quat(inNode.rotation[3], inNode.rotation[0], inNode.rotation[1], inNode.rotation[2]));

		if (inNode.has_scale)
			local_transform = glm::scale(local_transform, Vec3(inNode.scale[0], inNode.scale[1], inNode.scale[2]));
	}

	return local_transform;
}



void GltfImporter::ConvertBones(Entity inEntity, const cgltf_node& inNode)
{
	if (!m_GltfData->animations_count)
		return;

	if (!inNode.mesh || !inNode.skin)
		return;

	Mesh& mesh = m_Scene.Get<Mesh>(inEntity);
	Skeleton& skeleton = m_Scene.Add<Skeleton>(inEntity);

	for (const auto& [index, primitive] : gEnumerate(Slice(inNode.mesh->primitives, inNode.mesh->primitives_count)))
	{
		assert(primitive.type == cgltf_primitive_type_triangles);

		for (const auto& attribute : Slice(primitive.attributes, primitive.attributes_count))
		{
			if (attribute.type == cgltf_attribute_type_weights)
			{
				const cgltf_size float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
				Array<float> accessor_data(float_count);

				cgltf_float* data = accessor_data.data();
				cgltf_accessor_unpack_floats(attribute.data, data, float_count);
				const cgltf_size num_components = cgltf_num_components(attribute.data->type);

				for (uint32_t element_index = 0; element_index < attribute.data->count; element_index++)
				{
					skeleton.boneWeights.emplace_back(data[0], data[1], data[2], data[3]);
					data += num_components;
				}
			}

			else if (attribute.type == cgltf_attribute_type_joints)
			{
				const cgltf_size num_components = cgltf_num_components(attribute.data->type);
				uint32_t buffer[4]; // lets assume for now that its typically a vec4 of uint's

				// TODO: change bone indices from 'ivec4' to 'uvec4' as it makes more sense and we can pass it to read_uint directly
				for (uint32_t i = 0; i < attribute.data->count; i++)
				{
					IVec4& indices = skeleton.boneIndices.emplace_back();
					cgltf_accessor_read_uint(attribute.data, i, buffer, num_components);

					for (uint32_t j = 0; j < num_components; j++)
						indices[j] = buffer[j];
				}
			}
		}
	}

	cgltf_accessor* accessor = inNode.skin->inverse_bind_matrices;
	cgltf_size float_count = cgltf_accessor_unpack_floats(accessor, NULL, 0);


	Array<float> bind_matrix_data(float_count);
	cgltf_float* data_ptr = bind_matrix_data.data();
	cgltf_accessor_unpack_floats(accessor, data_ptr, float_count);
	
	const cgltf_size num_components = cgltf_num_components(accessor->type);

	for (uint32_t n = 0; n < accessor->count; n++)
	{
		Mat4x4& matrix = skeleton.boneOffsetMatrices.emplace_back(1.0f);
		memcpy(glm::value_ptr(matrix), data_ptr + n * num_components, num_components * sizeof(float));
	}

	skeleton.boneTransformMatrices.resize(skeleton.boneOffsetMatrices.size(), glm::mat4(1.0f));
	skeleton.boneWSTransformMatrices.resize(skeleton.boneOffsetMatrices.size(), glm::mat4(1.0f));

	if (m_Renderer)
		m_Renderer->UploadSkeletonBuffers(inEntity, skeleton, mesh);

	if (cgltf_node* root_bone = inNode.skin->skeleton)
	{
		skeleton.rootBone.name = root_bone->name;
		skeleton.rootBone.index = GetJointIndex(&inNode, root_bone);

		// recursive lambda to loop over the node hierarchy, dear lord help us all
		auto copyHierarchy = [&](auto&& copyHierarchy, cgltf_node* inCurrentNode, Skeleton::Bone& boneNode) -> void
		{
			for (cgltf_node* node : Slice(inCurrentNode->children, inCurrentNode->children_count))
			{
				const int index = GetJointIndex(&inNode, node);

				if (index != -1)
				{
					Skeleton::Bone& child_bone = boneNode.children.emplace_back();
					child_bone.name = node->name;
					child_bone.index = index;

					copyHierarchy(copyHierarchy, node, child_bone);
				}
			}
		};

		copyHierarchy(copyHierarchy, root_bone, skeleton.rootBone);
	}
}


int GltfImporter::GetJointIndex(const cgltf_node* inSkinNode, const cgltf_node* inJointNode)
{
	for (int index = 0; index < inSkinNode->skin->joints_count; index++)
	{
		if (inSkinNode->skin->joints[index] == inJointNode)
			return index;
	}

	return -1;
}


void GltfImporter::ConvertMaterial(Entity inEntity, const cgltf_material& gltfMaterial)
{
	Material& material = m_Scene.Add<Material>(inEntity);
	material.isTransparent = gltfMaterial.alpha_mode != cgltf_alpha_mode_opaque;

	if (gltfMaterial.has_pbr_metallic_roughness)
	{
		material.metallic = gltfMaterial.pbr_metallic_roughness.metallic_factor;
		material.roughness = gltfMaterial.pbr_metallic_roughness.roughness_factor;
		memcpy(glm::value_ptr(material.albedo), gltfMaterial.pbr_metallic_roughness.base_color_factor, sizeof(material.albedo));

		if (cgltf_texture* texture = gltfMaterial.pbr_metallic_roughness.base_color_texture.texture)
			material.albedoFile = TextureAsset::GetCachedPath(m_Directory.string() + texture->image->uri);

		if (cgltf_texture* texture = gltfMaterial.pbr_metallic_roughness.metallic_roughness_texture.texture)
		{
			String cached_texture_path = TextureAsset::GetCachedPath(m_Directory.string() + texture->image->uri);
			material.metallicFile = cached_texture_path;
			material.roughnessFile = cached_texture_path;
		}
	}

	if (cgltf_texture* normal_texture = gltfMaterial.normal_texture.texture)
		material.normalFile = TextureAsset::GetCachedPath(m_Directory.string() + normal_texture->image->uri);

	if (cgltf_texture* emissive_texture = gltfMaterial.emissive_texture.texture)
		material.emissiveFile = TextureAsset::GetCachedPath(m_Directory.string() + emissive_texture->image->uri);

	memcpy(glm::value_ptr(material.emissive), gltfMaterial.emissive_factor, sizeof(material.emissive));
}


} // raekor