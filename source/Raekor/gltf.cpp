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
    if (result != cgltf_result_success) {
        std::cerr << "[clgtf] Result error: " << cgltf_result_strings[result] << '\n';
        return false;
    }

    return true;
}


GltfImporter::~GltfImporter() { 
    if (m_GltfData) 
        cgltf_free(m_GltfData); 
}


bool GltfImporter::LoadFromFile(Assets& assets, const std::string& file) {
    /*
    * LOAD GLTF FROM DISK
    */
    m_Directory = fs::path(file).parent_path() / "";

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

        ConvertMaterial(m_Scene.emplace<Material>(entity), gltf_material);
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
            parseNode(*node, sInvalidEntity, glm::mat4(1.0f));

    const auto root_node = m_Scene.CreateSpatialEntity(fs::path(file).filename().string());
    
    for (const auto& entity : m_CreatedNodeEntities) {
        auto& node = m_Scene.get<Node>(entity);

        if (node.IsRoot())
            NodeSystem::sAppend(m_Scene, m_Scene.get<Node>(root_node), node);
    }

    return true;
}


void GltfImporter::parseNode(const cgltf_node& node, entt::entity parent, glm::mat4 transform) {
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

        ConvertMesh(m_Scene.emplace<Mesh>(entity), *node.mesh);

        if (node.skin && node.weights_count)
            ConvertBones(m_Scene.emplace<Skeleton>(entity), node);
    }

    for (const auto& child : Slice(node.children, node.children_count))
        parseNode(*child, parent, transform);
}


void GltfImporter::ConvertMesh(Mesh& mesh, const cgltf_mesh& gltfMesh) {
    mesh.uvs.reserve(gltfMesh.primitives_count);
    mesh.normals.reserve(gltfMesh.primitives_count);
    mesh.tangents.reserve(gltfMesh.primitives_count);
    mesh.positions.reserve(gltfMesh.primitives_count);
    mesh.indices.reserve(gltfMesh.primitives_count * 3);

    cgltf_material* material = nullptr;

    for(const auto& primitive : Slice(gltfMesh.primitives, gltfMesh.primitives_count)) {
        assert(primitive.type == cgltf_primitive_type_triangles);

        if (!material && primitive.material != material) {
            material = primitive.material;
        }
        else if (primitive.material != material) {
            material = primitive.material;

        }

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


void GltfImporter::ConvertBones(Skeleton& skeleton, const cgltf_node& assimpMesh) {
    if (!m_GltfData->animations_count)
        return;
    
    for (uint32_t anim_idx = 0; anim_idx < m_GltfData->animations_count; anim_idx++) {
        skeleton.animations.emplace_back(&m_GltfData->animations[anim_idx]);
    }

    skeleton.m_BoneWeights.resize(assimpMesh.weights_count);
    skeleton.m_BoneIndices.resize(assimpMesh.skin->joints_count);

    //for (uint32_t idx = 0; idx < assimpMesh->mNumBones; idx++) {
    //    auto bone = assimpMesh->mBones[idx];
    //    int bone_index = 0;

    //    if (skeleton.bonemapping.find(bone->mName.C_Str()) == skeleton.bonemapping.end()) {
    //        skeleton.m_BoneOffsets.push_back(Assimp::toMat4(bone->mOffsetMatrix));
    //        
    //        bone_index = skeleton.m_BoneOffsets.size() - 1;
    //        skeleton.bonemapping[bone->mName.C_Str()] = bone_index;
    //    } 
    //    else {
    //        bone_index = skeleton.bonemapping[bone->mName.C_Str()];
    //    }

    //    for (size_t j = 0; j < bone->mNumWeights; j++) {
    //        int vertexID = bone->mWeights[j].mVertexId;
    //        float weight = bone->mWeights[j].mWeight;

    //        for (int i = 0; i < 4; i++) {
    //            if (skeleton.m_BoneWeights[vertexID][i] == 0.0f) {
    //                skeleton.m_BoneIndices[vertexID][i] = bone_index;
    //                skeleton.m_BoneWeights[vertexID][i] = weight;
    //                break;
    //            }
    //        }
    //    }
    //}

    //skeleton.m_BoneTransforms.resize(skeleton.m_BoneOffsets.size());

    //if (m_UploadSkeletonCallback) {
    //    m_UploadSkeletonCallback(skeleton, mesh);
    //}

    //aiNode* root_bone = nullptr;
    //std::stack<aiNode*> nodes;
    //nodes.push(m_AiScene->mRootNode);

    //while (!nodes.empty() && !root_bone) {
    //    auto current = nodes.top();
    //    nodes.pop();

    //    const auto is_bone = skeleton.bonemapping.find(current->mName.C_Str()) != skeleton.bonemapping.end();

    //    if (is_bone) {
    //        assert(current->mParent);
    //        const auto has_parent_bone = skeleton.bonemapping.find(current->mParent->mName.C_Str()) != skeleton.bonemapping.end();

    //        if (!has_parent_bone) {
    //            root_bone = current;
    //            break;
    //        }
    //    }

    //    for (uint32_t i = 0; i < current->mNumChildren; i++) {
    //        nodes.push(current->mChildren[i]);
    //    }
    //}

    //assert(root_bone);
    //skeleton.m_Bones.name = root_bone->mName.C_Str();

    //// recursive lambda to loop over the node hierarchy
    //auto copyHierarchy = [&](auto&& copyHierarchy, aiNode* node, Bone& boneNode) -> void {
    //    for (unsigned int i = 0; i < node->mNumChildren; i++) {
    //        auto childNode = node->mChildren[i];

    //        if (skeleton.bonemapping.find(childNode->mName.C_Str()) != skeleton.bonemapping.end()) {
    //            auto& child = boneNode.children.emplace_back();
    //            child.name = childNode->mName.C_Str();
    //            copyHierarchy(copyHierarchy, childNode, child);
    //        }
    //    }
    //};

    //copyHierarchy(copyHierarchy, root_bone, skeleton.m_Bones);
}


void GltfImporter::ConvertMaterial(Material& material, const cgltf_material& gltfMaterial) {
    material.isTransparent = gltfMaterial.alpha_mode != cgltf_alpha_mode_opaque;

    if (gltfMaterial.has_pbr_metallic_roughness) {
        material.metallic = gltfMaterial.pbr_metallic_roughness.metallic_factor;
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