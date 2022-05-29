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
    cgltf_options options = {};
    if (!handle_cgltf_error(cgltf_parse_file(&options, file.c_str(), &m_GltfData)))
        return false;

    if (!handle_cgltf_error(cgltf_load_buffers(&options, m_GltfData, file.c_str())))
        return false;

    if (!handle_cgltf_error(cgltf_validate(m_GltfData)))
        return false;

    Timer timer;
    m_Directory = fs::path(file).parent_path() / "";

    // pre-parse materials and convert textures in parallel
    for (unsigned int i = 0; i < m_GltfData->materials_count; i++) {
        gPrintProgressBar("Converting material textures: ", float(i) / m_GltfData->materials_count);
        parseMaterial(m_GltfData->materials[i], m_Scene.create());
    }

    Async::sWait();

    std::cout << "Texture conversion took " << Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms. \n";

    // preload material texture in parallel
    m_Scene.LoadMaterialTextures(assets, Slice(m_Materials.data(), m_Materials.size()));

    for (const auto& scene : Slice(m_GltfData->scenes, m_GltfData->scenes_count))
        for (const auto& node : Slice(scene.nodes, scene.nodes_count))
            parseNode(*node, entt::null, m_Scene.CreateSpatialEntity(""));

    return true;
}


void GltfImporter::parseMaterial(cgltf_material& assimpMaterial, entt::entity entity) {
    auto& nameComponent = m_Scene.emplace<Name>(entity);

    if (assimpMaterial.name && strcmp(assimpMaterial.name, "") != 0)
        nameComponent.name = assimpMaterial.name;
    else
        nameComponent.name = "Material " + std::to_string(entt::to_integral(entity));

    LoadMaterial(entity, assimpMaterial);

    m_Materials.push_back(entity);
}


void GltfImporter::parseNode(const cgltf_node& node, entt::entity parent, entt::entity new_entity) {
    // name it after the node or its mesh
    if (node.name)
        m_Scene.get<Name>(new_entity).name = node.name;
    else if (node.mesh && node.mesh->name)
        m_Scene.get<Name>(new_entity).name = node.mesh->name;

    // set the new entity's parent
    if (parent != entt::null) {
        NodeSystem::sAppend(m_Scene,
            m_Scene.get<Node>(parent),
            m_Scene.get<Node>(new_entity)
        );
    }

    // gltf transformation to glm
    auto& transform = m_Scene.get<Transform>(new_entity);
    memcpy(glm::value_ptr(transform.localTransform), node.matrix, sizeof(node.matrix));
    transform.Decompose();

    // process meshes
    parseMeshes(node, new_entity, parent);

    // process children
    for (const auto& child : Slice(node.children, node.children_count)) {
        auto entity = m_Scene.CreateSpatialEntity(""); // Empty name because parseNode should set it
        parseNode(*child, new_entity, entity);
    }
}


void GltfImporter::parseMeshes(const cgltf_node& node, entt::entity new_entity, entt::entity parent) {
    if (node.mesh) {
        LoadMesh(new_entity, *node.mesh);

        if (node.weights_count)
            LoadBones(new_entity, node);
    }
}


void GltfImporter::LoadMesh(entt::entity entity, const cgltf_mesh& gltfMesh) {
    auto& mesh = m_Scene.emplace<Mesh>(entity);

    mesh.uvs.reserve(gltfMesh.primitives_count);
    mesh.normals.reserve(gltfMesh.primitives_count);
    mesh.tangents.reserve(gltfMesh.primitives_count);
    mesh.positions.reserve(gltfMesh.primitives_count);
    mesh.indices.reserve(gltfMesh.primitives_count * 3);

    for(const auto& primitive : Slice(gltfMesh.primitives, gltfMesh.primitives_count)) {
        assert(primitive.type == cgltf_primitive_type_triangles);

        for (int i = 0; i < m_GltfData->materials_count; i++)
            if (primitive.material == &m_GltfData->materials[i])
                mesh.material = m_Materials[i];

        for(const auto& attribute : Slice(primitive.attributes, primitive.attributes_count)) {
            const auto float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
            auto accessor_data = std::vector<float>(float_count); // TODO: allocate this once?
            
            cgltf_accessor_unpack_floats(attribute.data, accessor_data.data(), float_count);
            const auto num_components = cgltf_num_components(attribute.data->type);

            auto data = accessor_data.data();
            
            for (uint32_t element_index = 0; element_index < attribute.data->count; element_index++) {
                switch (attribute.type) {
                    case cgltf_attribute_type_position: {
                        mesh.positions.emplace_back(data[0], data[1], data[2]);
                    } break;
                    case cgltf_attribute_type_texcoord: {
                        mesh.uvs.emplace_back(data[0], data[1]);
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


void GltfImporter::LoadBones(entt::entity entity, const cgltf_node& assimpMesh) {
    if (!m_GltfData->animations_count)
        return;
    
    auto& mesh = m_Scene.get<Mesh>(entity);
    auto& skeleton = m_Scene.emplace<Skeleton>(entity);

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


void GltfImporter::LoadMaterial(entt::entity entity, const cgltf_material& gltfMaterial) {
    auto& material = m_Scene.emplace<Material>(entity);
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