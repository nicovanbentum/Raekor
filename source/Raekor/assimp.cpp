#include "pch.h"
#include "assimp.h"

#include "util.h"
#include "async.h"
#include "timer.h"
#include "scene.h"
#include "systems.h"
#include "components.h"
#include "application.h"

#ifndef DEPRECATE_ASSIMP

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

} // namespace Assimp


namespace Raekor {

bool AssimpImporter::LoadFromFile(const std::string& file, Assets* inAssets) {
    constexpr unsigned int flags =
        aiProcess_GenNormals |
        aiProcess_GenUVCoords |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_FindInstances |
        aiProcess_OptimizeGraph |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights |
        aiProcess_FindDegenerates |
        aiProcess_FindInvalidData |
        // aiProcess_OptimizeMeshes |
        // aiProcess_PreTransformVertices |
        aiProcess_ValidateDataStructure |
        // aiProcess_JoinIdenticalVertices |
        aiProcess_RemoveRedundantMaterials;

    // the importer takes care of deleting the scene
    m_Importer = std::make_shared<Assimp::Importer>();
    m_AiScene = m_Importer->ReadFile(file, flags);

    // error cases
    if (!m_AiScene || (!m_AiScene->HasMeshes() && !m_AiScene->HasMaterials())) {
        std::cerr << "[ASSIMP] Error loading " << file << ": " << m_Importer->GetErrorString() << '\n';
        return false;
    }

    Timer timer;
    m_Directory = Path(file).parent_path() / "";

    // pre-parse materials
    for (const auto& [index, material] : gEnumerate(Slice(m_AiScene->mMaterials, m_AiScene->mNumMaterials))) {
        auto entity = m_Scene.Create();
        m_Scene.Add<Name>(entity);
        m_Scene.Add<Material>(entity);
        m_Materials.push_back(entity);
    }

    for (const auto& [index, material] : gEnumerate(Slice(m_AiScene->mMaterials, m_AiScene->mNumMaterials))) {
        ParseMaterial(material, m_Materials[index]);
    }

    // preload material texture in parallel
    if (inAssets != nullptr)
        m_Scene.LoadMaterialTextures(*inAssets, Slice(m_Materials.data(), m_Materials.size()));

    // parse the node tree recursively
    auto root = m_Scene.CreateSpatialEntity(m_AiScene->mRootNode->mName.C_Str());
    ParseNode(m_AiScene->mRootNode, NULL_ENTITY, root);

    return true;
}


void AssimpImporter::ParseMaterial(aiMaterial* assimpMaterial, Entity entity) {
    auto& nameComponent = m_Scene.Get<Name>(entity);

    if (strcmp(assimpMaterial->GetName().C_Str(), "") != 0)
        nameComponent.name = assimpMaterial->GetName().C_Str();
    else
        nameComponent.name = "Material " + std::to_string(uint32_t(entity));

    LoadMaterial(entity, assimpMaterial);
}


void AssimpImporter::ParseNode(const aiNode* assimpNode, Entity parent, Entity new_entity) {
    // set the name
    m_Scene.Get<Name>(new_entity).name = assimpNode->mName.C_Str();

    // set the new entity's parent
    if (parent != NULL_ENTITY) {
        NodeSystem::sAppend(m_Scene,
            parent,
            m_Scene.Get<Node>(parent),
            new_entity,
            m_Scene.Get<Node>(new_entity)
        );
    }

    // translate assimp transformation to glm
    aiMatrix4x4 localTransform = assimpNode->mTransformation;
    auto& transform = m_Scene.Get<Transform>(new_entity);
    transform.localTransform = Assimp::toMat4(localTransform);
    transform.Decompose();

    // process meshes
    ParseMeshes(assimpNode, new_entity, parent);

    // process children
    for (uint32_t i = 0; i < assimpNode->mNumChildren; i++) {
        auto child = m_Scene.CreateSpatialEntity(assimpNode->mChildren[i]->mName.C_Str());
        ParseNode(assimpNode->mChildren[i], new_entity, child);
    }
}


void AssimpImporter::ParseMeshes(const aiNode* assimpNode, Entity new_entity, Entity parent) {
    for (uint32_t i = 0; i < assimpNode->mNumMeshes; i++) {
        auto entity = new_entity;
        const auto assimp_mesh = m_AiScene->mMeshes[assimpNode->mMeshes[i]];

        // separate entities for multiple meshes
        if (assimpNode->mNumMeshes > 1) {
            entity = m_Scene.CreateSpatialEntity(assimp_mesh->mName.C_Str());

            aiMatrix4x4 localTransform = assimpNode->mTransformation;
            auto& transform = m_Scene.Get<Transform>(entity);
            transform.localTransform = Assimp::toMat4(localTransform);
            transform.Decompose();

            auto p = parent != NULL_ENTITY ? parent : new_entity;
            NodeSystem::sAppend(m_Scene, p, m_Scene.Get<Node>(p), entity, m_Scene.Get<Node>(entity));
        }

        // process mesh
        LoadMesh(entity, assimp_mesh);

        // process bones
        if (assimp_mesh->HasBones())
            LoadBones(entity, assimp_mesh);
    }
}


void AssimpImporter::LoadMesh(Entity entity, const aiMesh* assimpMesh) {
    auto& mesh = m_Scene.Add<Mesh>(entity);

    mesh.positions.reserve(assimpMesh->mNumVertices);
    mesh.uvs.reserve(assimpMesh->mNumVertices);
    mesh.normals.reserve(assimpMesh->mNumVertices);
    mesh.tangents.reserve(assimpMesh->mNumVertices);

    for (size_t i = 0; i < assimpMesh->mNumVertices; i++) {
        mesh.positions.emplace_back(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z);

        if (assimpMesh->HasTextureCoords(0))
            mesh.uvs.emplace_back(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y);
        else
            mesh.uvs.emplace_back(0.0f, 0.0f);

        if (assimpMesh->HasNormals())
            mesh.normals.emplace_back(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z);

        if (assimpMesh->HasTangentsAndBitangents()) {
            auto& tangent = mesh.tangents.emplace_back(assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z);

            glm::vec3 bitangent = glm::vec3(assimpMesh->mBitangents[i].x, assimpMesh->mBitangents[i].y, assimpMesh->mBitangents[i].z);

            if (glm::dot(glm::cross(mesh.normals[i], glm::vec3(tangent)), bitangent) < 0.0f)
                tangent *= -1.0f;
        }
        else {
            mesh.tangents.emplace_back(0.0f, 0.0f, 0.0f);
        }
    }

    mesh.indices.reserve(assimpMesh->mNumFaces * 3);
    
    for (size_t i = 0; i < assimpMesh->mNumFaces; i++) {
        assert(assimpMesh->mFaces[i].mNumIndices == 3);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[0]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[1]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[2]);
    }

    mesh.CalculateAABB();

    //if (!assimpMesh->HasNormals() && !mesh.positions.empty())
    //    mesh.CalculateNormals();

    //if (!assimpMesh->HasTangentsAndBitangents() && !mesh.uvs.empty())
        //mesh.CalculateTangents();

    if(m_Renderer) 
        m_Renderer->UploadMeshBuffers(mesh);

    mesh.material = m_Materials[assimpMesh->mMaterialIndex];
}


void AssimpImporter::LoadBones(Entity entity, const aiMesh* assimpMesh) {
    if (!m_AiScene->HasAnimations())
        return;

    auto& mesh = m_Scene.Get<Mesh>(entity);
    auto& skeleton = m_Scene.Add<Skeleton>(entity);

    skeleton.boneWeights.resize(assimpMesh->mNumVertices);
    skeleton.boneIndices.resize(assimpMesh->mNumVertices);

    auto bone_map = std::unordered_map<std::string, uint32_t>();

    auto IsBone = [&](const std::string& inBoneName) {
        return bone_map.find(inBoneName) != bone_map.end();
    };

    for (const auto& [index, bone] : gEnumerate(Slice(assimpMesh->mBones, assimpMesh->mNumBones))) {
        bone_map.insert({ bone->mName.C_Str(), uint32_t(index) });
        skeleton.boneOffsetMatrices.push_back(Assimp::toMat4(bone->mOffsetMatrix));

        for (const auto& assimp_weight : Slice(bone->mWeights, bone->mNumWeights)) {
            auto& weight = skeleton.boneWeights[assimp_weight.mVertexId];
            auto& indices = skeleton.boneIndices[assimp_weight.mVertexId];

            for (int i = 0; i < weight.length(); i++) {
                if (weight[i] == 0.0f) {
                    weight[i] = assimp_weight.mWeight;
                    indices[i] = index;
                    break;
                }
                // vertex weights are stored as vec4, so if we were unable to use the last component, 
                // it means assimp somehow didn't limit vertex weights to 4 and we have excess weights.
                assert(i != 3);
            }
        }
    }

    skeleton.boneTransformMatrices.resize(skeleton.boneOffsetMatrices.size());

    if (m_Renderer)
        m_Renderer->UploadSkeletonBuffers(skeleton, mesh);

    aiNode* root_bone = nullptr;
    std::stack<aiNode*> nodes;
    nodes.push(m_AiScene->mRootNode);

    while (!nodes.empty() && !root_bone) {
        auto current = nodes.top();
        nodes.pop();

        if (IsBone(current->mName.C_Str())) {
            assert(current->mParent);

            if (!IsBone(current->mParent->mName.C_Str())) {
                root_bone = current;
                break;
            }
        }

        for (uint32_t i = 0; i < current->mNumChildren; i++)
            nodes.push(current->mChildren[i]);
    }

    assert(root_bone);
    skeleton.boneHierarchy.name = root_bone->mName.C_Str();
    skeleton.boneHierarchy.index = bone_map[skeleton.boneHierarchy.name];

    // recursive lambda to loop over the node hierarchy, dear lord help us all
    auto copyHierarchy = [&](auto&& copyHierarchy, aiNode* inNode, Bone& boneNode) -> void
    {
        for (auto node : Slice(inNode->mChildren, inNode->mNumChildren)) {

            if (IsBone(node->mName.C_Str())) {
                auto& child = boneNode.children.emplace_back();
                child.name = node->mName.C_Str();
                child.index = bone_map[child.name];

                copyHierarchy(copyHierarchy, node, child);
            }
        }
    };

    copyHierarchy(copyHierarchy, root_bone, skeleton.boneHierarchy);

    for (const auto& ai_animation : Slice(m_AiScene->mAnimations, m_AiScene->mNumAnimations)) {
        auto& animation = skeleton.animations.emplace_back(ai_animation);

        for (const auto& channel : Slice(ai_animation->mChannels, ai_animation->mNumChannels))
            animation.LoadKeyframes(bone_map[channel->mNodeName.C_Str()], channel);
    }
}


void AssimpImporter::LoadMaterial(Entity entity, const aiMaterial* assimpMaterial) {
    auto& material = m_Scene.Get<Material>(entity);

    aiString albedoFile, normalmapFile, metalroughFile;
    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
    assimpMaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

    aiString ai_alpha_mode;
    if (assimpMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, ai_alpha_mode) == AI_SUCCESS)
        if (strcmp(ai_alpha_mode.C_Str(), "MASK") == 0 || strcmp(ai_alpha_mode.C_Str(), "BLEND") == 0)
            material.isTransparent = true;

    aiColor4D diffuse;
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
        material.albedo = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };

    aiColor4D emissive;
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_EMISSIVE, &emissive))
        material.emissive = { emissive.r, emissive.g, emissive.b };

    float roughness, metallic;
    if (AI_SUCCESS == aiGetMaterialFloat(assimpMaterial, AI_MATKEY_METALLIC_FACTOR, &metallic))
        material.metallic = metallic;

    if (AI_SUCCESS == aiGetMaterialFloat(assimpMaterial, AI_MATKEY_ROUGHNESS_FACTOR, &roughness))
        material.roughness = roughness;

    if (albedoFile.length)
        material.albedoFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + albedoFile.C_Str());

    if (normalmapFile.length)
        material.normalFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + normalmapFile.C_Str());

    if (metalroughFile.length)
        material.metalroughFile = TextureAsset::sAssetsToCachedPath(m_Directory.string() + metalroughFile.C_Str());
}

} // raekor

#endif // DEPRECATE_ASSIMP