#include "pch.h"
#include "assimp.h"

#include "util.h"
#include "scene.h"
#include "systems.h"
#include "async.h"
#include "timer.h"

namespace Assimp {

glm::mat4 toMat4(const aiMatrix4x4& from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
};

} // assimp

namespace Raekor {

bool AssimpImporter::LoadFromFile(Assets& assets, const std::string& file) {
    constexpr unsigned int flags =
        //aiProcess_PreTransformVertices |
        aiProcess_FindInstances |
        aiProcess_OptimizeGraph |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_LimitBoneWeights |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_ValidateDataStructure;

    // the importer takes care of deleting the scene
    auto importer = std::make_shared<Assimp::Importer>();
    assimpScene = importer->ReadFile(file, flags);

    // error cases
    if (!assimpScene || (!assimpScene->HasMeshes() && !assimpScene->HasMaterials())) {
        std::cerr << "[ASSIMP] Error loading " << file << ": " << importer->GetErrorString() << '\n';
        return false;
    }

    auto path = fs::path(file);
    directory = path.parent_path() / "";

    // pre-parse materials
    Timer timer;

    for (unsigned int i = 0; i < assimpScene->mNumMaterials; i++) {
        printProgressBar("Converting material textures: ", float(i) / assimpScene->mNumMaterials);
        parseMaterial(assimpScene->mMaterials[i], scene.create());
    }

    std::cout << "Texture conversion took " << Timer::ToMilliseconds(timer.GetElapsedTime()) << " ms. \n";

    // preload material texture in parallel
    scene.loadMaterialTextures(assets, materials);

    // parse the node tree recursively
    auto root = scene.createObject(assimpScene->mRootNode->mName.C_Str());
    parseNode(assimpScene->mRootNode, entt::null, root);

return true;
}



void AssimpImporter::parseMaterial(aiMaterial* assimpMaterial, entt::entity entity) {
    // figure out the material name
    auto& nameComponent = scene.emplace<Name>(entity);

    if (strcmp(assimpMaterial->GetName().C_Str(), "") != 0) {
        nameComponent.name = assimpMaterial->GetName().C_Str();
    } else {
        nameComponent.name = "Material " + std::to_string(entt::to_integral(entity));
    }

    // process the material
    LoadMaterial(entity, assimpMaterial);
    materials.push_back(entity);
}



void AssimpImporter::parseNode(const aiNode* assimpNode, entt::entity parent, entt::entity new_entity) {
    // set the name
    scene.get<Name>(new_entity).name = assimpNode->mName.C_Str();

    // set the new entity's parent
    if (parent != entt::null) {
        NodeSystem::append(scene,
            scene.get<Node>(parent),
            scene.get<Node>(new_entity)
        );
    }

    // translate assimp transformation to glm
    aiMatrix4x4 localTransform = assimpNode->mTransformation;
    auto& transform = scene.get<Transform>(new_entity);
    transform.localTransform = Assimp::toMat4(localTransform);
    transform.decompose();

    // process meshes
    parseMeshes(assimpNode, new_entity, parent);

    // process children
    for (uint32_t i = 0; i < assimpNode->mNumChildren; i++) {
        auto child = scene.createObject(assimpNode->mChildren[i]->mName.C_Str());
        parseNode(assimpNode->mChildren[i], new_entity, child);
    }
}



void AssimpImporter::parseMeshes(const aiNode* assimpNode, entt::entity new_entity, entt::entity parent) {
    for (uint32_t i = 0; i < assimpNode->mNumMeshes; i++) {
        auto entity = new_entity;
        const aiMesh* assimpMesh = assimpScene->mMeshes[assimpNode->mMeshes[i]];

        // separate entities for multiple meshes
        if (assimpNode->mNumMeshes > 1) {
            entity = scene.createObject(assimpMesh->mName.C_Str());

            aiMatrix4x4 localTransform = assimpNode->mTransformation;
            auto& transform = scene.get<Transform>(entity);
            transform.localTransform = Assimp::toMat4(localTransform);
            transform.decompose();

            auto p = parent != entt::null ? parent : new_entity;
            NodeSystem::append(
                scene,
                scene.get<Node>(p),
                scene.get<Node>(entity)
            );
        }

        // process mesh
        LoadMesh(entity, assimpMesh);

        // process bones
        if (assimpMesh->HasBones()) {
            LoadBones(entity, assimpMesh);
        }

    }
}



void AssimpImporter::LoadMesh(entt::entity entity, const aiMesh* assimpMesh) {
    auto& mesh = scene.emplace<Mesh>(entity);

    mesh.positions.reserve(assimpMesh->mNumVertices);
    mesh.uvs.reserve(assimpMesh->mNumVertices);
    mesh.normals.reserve(assimpMesh->mNumVertices);
    mesh.tangents.reserve(assimpMesh->mNumVertices);

    for (size_t i = 0; i < assimpMesh->mNumVertices; i++) {
        mesh.positions.emplace_back(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z);

        if (assimpMesh->HasTextureCoords(0)) {
            mesh.uvs.emplace_back(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y);
        }

        if (assimpMesh->HasNormals()) {
            mesh.normals.emplace_back(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z);
        }

        if (assimpMesh->HasTangentsAndBitangents()) {
            auto& tangent = mesh.tangents.emplace_back(assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z);

            glm::vec3 bitangent = glm::vec3(assimpMesh->mBitangents[i].x, assimpMesh->mBitangents[i].y, assimpMesh->mBitangents[i].z);

            if (glm::dot(glm::cross(mesh.normals[i],glm::vec3(tangent)), bitangent) < 0.0f) {
                tangent *= -1.0f;
            }
        }
    }

    mesh.indices.reserve(assimpMesh->mNumFaces * 3);
    
    for (size_t i = 0; i < assimpMesh->mNumFaces; i++) {
        assert(assimpMesh->mFaces[i].mNumIndices == 3);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[0]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[1]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[2]);
    }

    mesh.generateAABB();

    if(m_UploadMeshCallback) m_UploadMeshCallback(mesh);

    mesh.material = materials[assimpMesh->mMaterialIndex];
}



void AssimpImporter::LoadBones(entt::entity entity, const aiMesh* assimpMesh) {
    if (!assimpScene->HasAnimations()) {
        return;
    }
    
    auto& mesh = scene.get<Mesh>(entity);
    auto& skeleton = scene.emplace<Skeleton>(entity);

    for (uint32_t anim_idx = 0; anim_idx < assimpScene->mNumAnimations; anim_idx++) {
        skeleton.animations.emplace_back(assimpScene->mAnimations[anim_idx]);
    }

    skeleton.m_BoneWeights.resize(assimpMesh->mNumVertices);
    skeleton.m_BoneIndices.resize(assimpMesh->mNumVertices);

    for (uint32_t idx = 0; idx < assimpMesh->mNumBones; idx++) {
        auto bone = assimpMesh->mBones[idx];
        int bone_index = 0;

        if (skeleton.bonemapping.find(bone->mName.C_Str()) == skeleton.bonemapping.end()) {
            skeleton.m_BoneOffsets.push_back(Assimp::toMat4(bone->mOffsetMatrix));
            
            bone_index = skeleton.m_BoneOffsets.size() - 1;
            skeleton.bonemapping[bone->mName.C_Str()] = bone_index;
        } 
        else {
            bone_index = skeleton.bonemapping[bone->mName.C_Str()];
        }

        for (size_t j = 0; j < bone->mNumWeights; j++) {
            int vertexID = bone->mWeights[j].mVertexId;
            float weight = bone->mWeights[j].mWeight;

            for (int i = 0; i < 4; i++) {
                if (skeleton.m_BoneWeights[vertexID][i] == 0.0f) {
                    skeleton.m_BoneIndices[vertexID][i] = bone_index;
                    skeleton.m_BoneWeights[vertexID][i] = weight;
                    break;
                }
            }
        }
    }

    skeleton.m_BoneTransforms.resize(skeleton.m_BoneOffsets.size());

    if (m_UploadSkeletonCallback) {
        m_UploadSkeletonCallback(skeleton, mesh);
    }

    aiNode* root_bone = nullptr;
    std::stack<aiNode*> nodes;
    nodes.push(assimpScene->mRootNode);

    while (!nodes.empty() && !root_bone) {
        auto current = nodes.top();
        nodes.pop();

        const auto is_bone = skeleton.bonemapping.find(current->mName.C_Str()) != skeleton.bonemapping.end();

        if (is_bone) {
            assert(current->mParent);
            const auto has_parent_bone = skeleton.bonemapping.find(current->mParent->mName.C_Str()) != skeleton.bonemapping.end();

            if (!has_parent_bone) {
                root_bone = current;
                break;
            }
        }

        for (uint32_t i = 0; i < current->mNumChildren; i++) {
            nodes.push(current->mChildren[i]);
        }
    }

    assert(root_bone);
    skeleton.m_Bones.name = root_bone->mName.C_Str();

    // recursive lambda to loop over the node hierarchy
    auto copyHierarchy = [&](auto&& copyHierarchy, aiNode* node, Bone& boneNode) -> void {
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            auto childNode = node->mChildren[i];

            if (skeleton.bonemapping.find(childNode->mName.C_Str()) != skeleton.bonemapping.end()) {
                auto& child = boneNode.children.emplace_back();
                child.name = childNode->mName.C_Str();
                copyHierarchy(copyHierarchy, childNode, child);
            }
        }
    };

    copyHierarchy(copyHierarchy, root_bone, skeleton.m_Bones);
}



void AssimpImporter::LoadMaterial(entt::entity entity, const aiMaterial* assimpMaterial) {
    auto& material = scene.emplace<Material>(entity);

    aiString albedoFile, normalmapFile, metalroughFile;
    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
    assimpMaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

    aiColor4D diffuse;
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
        material.albedo = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };
    }

    aiColor4D emissive;
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_EMISSIVE, &emissive)) {
        material.emissive = { emissive.r, emissive.g, emissive.b };
    }

    float roughness, metallic;
    if (AI_SUCCESS == aiGetMaterialFloat(assimpMaterial, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &metallic)) {
        material.metallic = metallic;
    }

    if (AI_SUCCESS == aiGetMaterialFloat(assimpMaterial, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &roughness)) {
        material.roughness = roughness;
    }

    std::error_code ec;
    if (albedoFile.length) {
        Async::dispatch([&]() {
            auto assetPath = TextureAsset::convert(directory.string() + albedoFile.C_Str());
            material.albedoFile = assetPath;
        });
    }
    if (normalmapFile.length) {
        Async::dispatch([&]() {
            auto assetPath = TextureAsset::convert(directory.string() + normalmapFile.C_Str());
            material.normalFile = assetPath;
        });
    }
    if (metalroughFile.length) {
        Async::dispatch([&]() {
            auto assetPath = TextureAsset::convert(directory.string() + metalroughFile.C_Str());
            material.metalroughFile = assetPath;
        });
    }

    Async::wait();
}

} // raekor