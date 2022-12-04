#include "pch.h"
#include "gltf.h"

#include "util.h"
#include "scene.h"
#include "systems.h"
#include "async.h"
#include "timer.h"

namespace Raekor {

constexpr std::array cgltf_result_strings = {
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


bool handle_cgltf_error(cgltf_result result) {
    if (result == cgltf_result_success)
        return true;

    gWarn(cgltf_result_strings[result]);
    return false;
}


GltfImporter::~GltfImporter() { 
    if (m_GltfData) 
        cgltf_free(m_GltfData); 
}


bool GltfImporter::LoadFromFile(Assets& assets, const std::string& file) {
    /*
    * LOAD GLTF FROM DISK
    */
    m_Directory = Path(file).parent_path() / "";

    cgltf_options options = {};
    if (!handle_cgltf_error(cgltf_parse_file(&options, file.c_str(), &m_GltfData)))
        return false;

    if (!handle_cgltf_error(cgltf_load_buffers(&options, m_GltfData, file.c_str())))
        return false;

    if (!handle_cgltf_error(cgltf_validate(m_GltfData)))
        return false;

    /*
    * PARSE MATERIALS
    */
    Timer timer;
    for (unsigned int index = 0; index < m_GltfData->materials_count; index++) {
        auto& gltf_material = m_GltfData->materials[index];

        auto entity = m_Scene.create();
        auto& nameComponent = m_Scene.emplace<Name>(entity);

        if (gltf_material.name && strcmp(gltf_material.name, "") != 0)
            nameComponent.name = gltf_material.name;
        else
            nameComponent.name = "Material " + std::to_string(entt::to_integral(entity));

        ConvertMaterial(entity, gltf_material);
        m_Materials.push_back(entity);
        
        gPrintProgressBar("DDS Conversion Progress: ", float(index) / m_GltfData->materials_count);
    }

    Async::sWait();
    std::cout << "DDS Conversion: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms. \n";
    m_Scene.LoadMaterialTextures(assets, Slice(m_Materials.data(), m_Materials.size()));

    /*
    * PARSE NODES & MESHES
    */
    for (const auto& scene : Slice(m_GltfData->scenes, m_GltfData->scenes_count))
        for (const auto& node : Slice(scene.nodes, scene.nodes_count))
            ParseNode(*node, sInvalidEntity, glm::mat4(1.0f));

    const auto root_node = m_Scene.CreateSpatialEntity(Path(file).filename().string());
    
    for (const auto& entity : m_CreatedNodeEntities) {
        auto& node = m_Scene.get<Node>(entity);

        if (node.IsRoot())
            NodeSystem::sAppend(m_Scene, m_Scene.get<Node>(root_node), node);
    }

    return true;
}


void GltfImporter::ParseNode(const cgltf_node& node, entt::entity parent, glm::mat4 transform) {
    glm::mat4 local_transform;
    memcpy(glm::value_ptr(local_transform), node.matrix, sizeof(node.matrix));
    transform *= local_transform;

    if (node.mesh) {
        auto entity = m_CreatedNodeEntities.emplace_back(m_Scene.CreateSpatialEntity());

        // name it after the node or its mesh
        if (node.name)
            m_Scene.get<Name>(entity).name = node.name;
        else if (node.mesh && node.mesh->name)
            m_Scene.get<Name>(entity).name = node.mesh->name;
        else 
            m_Scene.get<Name>(entity).name = "Mesh " + std::to_string(entt::to_integral(entity));

        auto& mesh_transform = m_Scene.get<Transform>(entity);
        mesh_transform.localTransform = transform;
        mesh_transform.Decompose();

        // set the new entity's parent
        if (parent != entt::null)
            NodeSystem::sAppend(m_Scene, m_Scene.get<Node>(parent), m_Scene.get<Node>(entity));

        ConvertMesh(entity, *node.mesh);

        if (node.skin)
            ConvertBones(entity, node);
    }

    for (const auto& child : Slice(node.children, node.children_count))
        ParseNode(*child, parent, transform);
}


void GltfImporter::ConvertMesh(Entity inEntity, const cgltf_mesh& gltfMesh) {
    auto& mesh = m_Scene.emplace<Mesh>(inEntity);

    mesh.uvs.reserve(gltfMesh.primitives_count);
    mesh.normals.reserve(gltfMesh.primitives_count);
    mesh.tangents.reserve(gltfMesh.primitives_count);
    mesh.positions.reserve(gltfMesh.primitives_count);
    mesh.indices.reserve(gltfMesh.primitives_count * 3);

    std::unordered_set<cgltf_material*> material_ptrs;

    for (const auto& primitive : Slice(gltfMesh.primitives, gltfMesh.primitives_count))
        material_ptrs.insert(primitive.material);

    if (material_ptrs.size() > 1)
        gWarn("Gltf mesh uses different materials per primitive which is currently not supported.");

    for(const auto& primitive : Slice(gltfMesh.primitives, gltfMesh.primitives_count)) {
        assert(primitive.type == cgltf_primitive_type_triangles);

        for (int i = 0; i < m_GltfData->materials_count; i++)
            if (primitive.material == &m_GltfData->materials[i])
                mesh.material = m_Materials[i];

        for(const auto& attribute : Slice(primitive.attributes, primitive.attributes_count)) {
            const auto float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
            auto accessor_data = std::vector<float>(float_count); // TODO: allocate this once?
            
            auto data = accessor_data.data();
            cgltf_accessor_unpack_floats(attribute.data, data, float_count);
            const auto num_components = cgltf_num_components(attribute.data->type);

            for (uint32_t element_index = 0; element_index < attribute.data->count; element_index++) {
                switch (attribute.type) {
                    case cgltf_attribute_type_position: {
                        mesh.positions.emplace_back(data[0], data[1], data[2]);
                    } break;
                    case cgltf_attribute_type_texcoord: {
                        mesh.uvs.emplace_back(data[0], data[1] * -1.0f);
                    } break;
                    case cgltf_attribute_type_normal: {
                        mesh.normals.emplace_back(data[0], data[1], data[2]);
                    } break;
                    case cgltf_attribute_type_tangent: {
                        mesh.tangents.emplace_back(data[0], data[1], data[2]);
                    } break;
                }

                data += num_components;
            }
            // TODO: flip tangents
        }

        for (uint32_t i = 0; i < primitive.indices->count; i++)
            mesh.indices.emplace_back(uint32_t(cgltf_accessor_read_index(primitive.indices, i)));
    }

    mesh.CalculateAABB();

    if (mesh.normals.empty() && !mesh.positions.empty())
        mesh.CalculateNormals();

    if (mesh.tangents.empty() && !mesh.uvs.empty())
        mesh.CalculateTangents();

    if(m_UploadMeshCallback) 
        m_UploadMeshCallback(mesh);
}

void GltfImporter::ConvertBones(Entity inEntity, const cgltf_node& inNode) {
    if (!m_GltfData->animations_count)
        return;

    if (!inNode.mesh || !inNode.skin)
        return;

    auto& mesh = m_Scene.get<Mesh>(inEntity);
    auto& skeleton = m_Scene.emplace<Skeleton>(inEntity);
    

    for (const auto& [index, primitive] : gEnumerate(Slice(inNode.mesh->primitives, inNode.mesh->primitives_count))) {
        
        assert(primitive.type == cgltf_primitive_type_triangles);

        for (const auto& attribute : Slice(primitive.attributes, primitive.attributes_count)) {
            
            if (attribute.type == cgltf_attribute_type_weights) {
                const auto float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
                auto accessor_data = std::vector<float>(float_count);

                auto data = accessor_data.data();
                cgltf_accessor_unpack_floats(attribute.data, data, float_count);
                const auto num_components = cgltf_num_components(attribute.data->type);

                for (uint32_t element_index = 0; element_index < attribute.data->count; element_index++) {
                    skeleton.boneWeights.emplace_back(data[0], data[1], data[2], data[3]);
                    data += num_components;
                }
            }

            else if (attribute.type == cgltf_attribute_type_joints) {
                const auto num_components = cgltf_num_components(attribute.data->type);
                uint32_t buffer[4]; // lets assume for now that its typically a vec4 of uint's

                // TODO: change bone indices from 'ivec4' to 'uvec4' as it makes more sense and we can pass it to read_uint directly
                for (uint32_t i = 0; i < attribute.data->count; i++) {
                    auto& indices = skeleton.boneIndices.emplace_back();
                    cgltf_accessor_read_uint(attribute.data, i, buffer, num_components);

                    for (uint32_t j = 0; j < num_components; j++)
                        indices[j] = buffer[j];
                }
            }
        }
    }

    auto accessor          = inNode.skin->inverse_bind_matrices;
    const auto float_count = cgltf_accessor_unpack_floats(accessor, NULL, 0);
    auto bind_matrix_data  = std::vector<float>(float_count);
    auto data_ptr          = bind_matrix_data.data();
    cgltf_accessor_unpack_floats(accessor, data_ptr, float_count);
    const auto num_components = cgltf_num_components(accessor->type);

    for (uint32_t n = 0; n < accessor->count; n++) {
        auto& matrix = skeleton.boneOffsetMatrices.emplace_back(1.0f);
        memcpy(glm::value_ptr(matrix), data_ptr + n * num_components, num_components * sizeof(float));
    }

    skeleton.boneTransformMatrices.resize(skeleton.boneOffsetMatrices.size());

    if (m_UploadSkeletonCallback)
        m_UploadSkeletonCallback(skeleton, mesh);

    auto root_bone = inNode.skin->skeleton;
    skeleton.boneHierarchy.name = root_bone->name;
    skeleton.boneHierarchy.index = GetJointIndex(&inNode, root_bone);

    // recursive lambda to loop over the node hierarchy, dear lord help us all
    auto copyHierarchy = [&](auto&& copyHierarchy, cgltf_node* inCurrentNode, Bone& boneNode) -> void
    {
        for (auto node : Slice(inCurrentNode->children, inCurrentNode->children_count)) {
            const auto index = GetJointIndex(&inNode, node);

            if (index != -1) {
                auto& child = boneNode.children.emplace_back();
                child.name = node->name;
                child.index = index;

                copyHierarchy(copyHierarchy, node, child);
            }
        }
    };

    copyHierarchy(copyHierarchy, root_bone, skeleton.boneHierarchy);

    for (const auto& gltf_animation : Slice(m_GltfData->animations, m_GltfData->animations_count)) {
        auto& animation = skeleton.animations.emplace_back(&gltf_animation);
    
        for (const auto& channel : Slice(gltf_animation.channels, gltf_animation.channels_count)) {
            const auto joint_index = GetJointIndex(&inNode, channel.target_node);

            if (animation.m_BoneAnimations.find(joint_index) == animation.m_BoneAnimations.end())
                animation.m_BoneAnimations[joint_index] = KeyFrames(joint_index);

            auto& keyframes = animation.m_BoneAnimations[joint_index];

            animation.m_TotalDuration = glm::max(animation.m_TotalDuration, Timer::sToMilliseconds(channel.sampler->input->max[0]));

            assert(channel.sampler->interpolation == cgltf_interpolation_type_linear);
            float buffer[4]; // covers time (scalar float values), pos and scale (vec3) and rotation (quat)
            
            switch (channel.target_path) {
            case cgltf_animation_path_type_translation: {
                for (uint32_t index = 0; index < channel.sampler->input->count; index++) {
                    auto& key = keyframes.positionKeys.emplace_back();

                    const auto num_components = cgltf_num_components(channel.sampler->input->type);
                    assert(num_components == 1);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mTime = Timer::sToMilliseconds(buffer[0]);
                }

                for (uint32_t index = 0; index < channel.sampler->output->count; index++) {
                    assert(index < keyframes.positionKeys.size());
                    auto& key = keyframes.positionKeys[index];

                    const auto num_components = cgltf_num_components(channel.sampler->output->type);
                    assert(num_components == 3);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mValue = aiVector3D(buffer[0], buffer[1], buffer[2]);
                }

            } break;
            case cgltf_animation_path_type_rotation: {
                for (uint32_t index = 0; index < channel.sampler->input->count; index++) {
                    auto& key = keyframes.rotationkeys.emplace_back();

                    const auto num_components = cgltf_num_components(channel.sampler->input->type);
                    assert(num_components == 1);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mTime = Timer::sToMilliseconds(buffer[0]);
                }

                for (uint32_t index = 0; index < channel.sampler->output->count; index++) {
                    assert(index < keyframes.rotationkeys.size());
                    auto& key = keyframes.rotationkeys[index];

                    const auto num_components = cgltf_num_components(channel.sampler->output->type);
                    assert(num_components == 4);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mValue = aiQuaternion(buffer[3], buffer[0], buffer[1], buffer[2]);
                }
            } break;
            case cgltf_animation_path_type_scale: {
                for (uint32_t index = 0; index < channel.sampler->input->count; index++) {
                    auto& key = keyframes.scaleKeys.emplace_back();

                    const auto num_components = cgltf_num_components(channel.sampler->input->type);
                    assert(num_components == 1);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mTime = Timer::sToMilliseconds(buffer[0]);
                }

                for (uint32_t index = 0; index < channel.sampler->output->count; index++) {
                    assert(index < keyframes.scaleKeys.size());
                    auto& key = keyframes.scaleKeys[index];

                    const auto num_components = cgltf_num_components(channel.sampler->output->type);
                    assert(num_components == 3);

                    cgltf_accessor_read_float(channel.sampler->input, index, buffer, num_components);
                    key.mValue = aiVector3D(buffer[0], buffer[1], buffer[2]);
                }
            } break;
            }
        }
    }

    for (auto& animation : skeleton.animations) {
        for (auto& [joint_index, keyframes] : animation.m_BoneAnimations) {
            const auto max = glm::max(glm::max(keyframes.positionKeys.size(), keyframes.rotationkeys.size()), keyframes.scaleKeys.size());

            if (max > 0) {
                if (keyframes.positionKeys.empty()) 
                    keyframes.positionKeys.emplace_back();

                if (keyframes.rotationkeys.empty()) 
                    keyframes.rotationkeys.emplace_back();

                if (keyframes.scaleKeys.empty()) 
                    keyframes.scaleKeys.emplace_back();
            }
        }
    }
}


int GltfImporter::GetJointIndex(const cgltf_node* inSkinNode, const cgltf_node* inJointNode) {
    for (int index = 0; index < inSkinNode->skin->joints_count; index++) {
        if (inSkinNode->skin->joints[index] == inJointNode)
            return index;
    }
    
    return -1;
}


void GltfImporter::ConvertMaterial(Entity inEntity, const cgltf_material& gltfMaterial) {
    auto& material = m_Scene.emplace<Material>(inEntity);
    material.isTransparent = gltfMaterial.alpha_mode != cgltf_alpha_mode_opaque;

    if (gltfMaterial.has_pbr_metallic_roughness) {
        material.metallic  = gltfMaterial.pbr_metallic_roughness.metallic_factor;
        material.roughness = gltfMaterial.pbr_metallic_roughness.roughness_factor;
        memcpy(glm::value_ptr(material.albedo), gltfMaterial.pbr_metallic_roughness.base_color_factor, sizeof(material.albedo));

        if (auto texture = gltfMaterial.pbr_metallic_roughness.base_color_texture.texture)
            material.albedoFile = texture->image->uri;

        if (auto texture = gltfMaterial.pbr_metallic_roughness.metallic_roughness_texture.texture)
            material.metalroughFile = texture->image->uri;
    }

    if (auto normal_texture = gltfMaterial.normal_texture.texture)
        material.normalFile = normal_texture->image->uri;

    memcpy(glm::value_ptr(material.emissive), gltfMaterial.emissive_factor, sizeof(material.emissive));

    if (!material.albedoFile.empty()) {
        Async::sQueueJob([this, &material]() {
            auto assetPath = TextureAsset::sConvert(m_Directory.string() + material.albedoFile);
            material.albedoFile = assetPath;
        });
    }

    if (!material.normalFile.empty()) {
        Async::sQueueJob([this, &material]() {
            auto assetPath = TextureAsset::sConvert(m_Directory.string() + material.normalFile);
            material.normalFile = assetPath;
        });
    }

    if (!material.metalroughFile.empty()) {
        Async::sQueueJob([this, &material]() {
            auto assetPath = TextureAsset::sConvert(m_Directory.string() + material.metalroughFile);
            material.metalroughFile = assetPath;
        });
    }
}

} // raekor