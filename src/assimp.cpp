#include "pch.h"
#include "assimp.h"

#include "scene.h"
#include "systems.h"

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

bool AssimpImporter::LoadFromFile(const std::string& file, AssetManager& assetManager) {
    constexpr unsigned int flags =
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_ValidateDataStructure;

    // the importer takes care of deleting the scene
    auto importer = std::make_shared<Assimp::Importer>();
    assimpScene = importer->ReadFile(file, flags);

    // error cases
    if (!assimpScene || (!assimpScene->HasMeshes() && !assimpScene->HasMaterials())) {
        std::cerr << "[IMPORT] Error loading " << file << ": " << importer->GetErrorString() << '\n';
        return false;
    }

    auto path = std::filesystem::path(file);
    directory = path.parent_path() / "";

    // pre-parse materials
    for (unsigned int i = 0; i < assimpScene->mNumMaterials; i++) {
        printProgressBar(i, 0, assimpScene->mNumMaterials);
        parseMaterial(assimpScene->mMaterials[i], scene->Create());
    }

    // preload material texture in parallel
    scene->loadMaterialTextures(materials, assetManager);

    // parse the node tree recursively
    auto root = scene->createObject(assimpScene->mRootNode->mName.C_Str());
    parseNode(assimpScene->mRootNode, entt::null, root);

    return true;
}

void AssimpImporter::parseMaterial(aiMaterial* assimpMaterial, entt::entity entity) {
    // figure out the material name
    auto& nameComponent = scene->Add<ecs::NameComponent>(entity);

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
    scene->Get<ecs::NameComponent>(new_entity).name = assimpNode->mName.C_Str();

    // set the new entity's parent
    if (parent != entt::null) {
        NodeSystem::append(
            *scene,
            scene->Get<ecs::NodeComponent>(parent),
            scene->Get<ecs::NodeComponent>(new_entity)
        );
    }

    // translate assimp transformation to glm
    aiMatrix4x4 localTransform = assimpNode->mTransformation;
    auto& transform = scene->Get<ecs::TransformComponent>(new_entity);
    transform.localTransform = Assimp::toMat4(localTransform);
    transform.decompose();

    // process meshes
    parseMeshes(assimpNode, new_entity, parent);

    // process children
    for (uint32_t i = 0; i < assimpNode->mNumChildren; i++) {
        auto child = scene->createObject(assimpNode->mChildren[i]->mName.C_Str());
        parseNode(assimpNode->mChildren[i], new_entity, child);
    }
}

void AssimpImporter::parseMeshes(const aiNode* assimpNode, entt::entity new_entity, entt::entity parent) {
    for (uint32_t i = 0; i < assimpNode->mNumMeshes; i++) {
        auto entity = new_entity;
        const aiMesh* assimpMesh = assimpScene->mMeshes[assimpNode->mMeshes[i]];

        // separate entities for multiple meshes
        if (assimpNode->mNumMeshes > 1) {
            entity = scene->createObject(assimpMesh->mName.C_Str());

            aiMatrix4x4 localTransform = assimpNode->mTransformation;
            auto& transform = scene->Get<ecs::TransformComponent>(entity);
            transform.localTransform = Assimp::toMat4(localTransform);
            transform.decompose();

                auto p = parent != entt::null ? parent : new_entity;
                NodeSystem::append(
                    *scene,
                    scene->Get<ecs::NodeComponent>(p),
                    scene->Get<ecs::NodeComponent>(entity)
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

/////////////////////////////////////////////////////////////////////////////////////////

void AssimpImporter::LoadMesh(entt::entity entity, const aiMesh* assimpMesh) {
    // extract vertices
    auto& mesh = scene->Add<ecs::MeshComponent>(entity);

    for (size_t i = 0; i < assimpMesh->mNumVertices; i++) {
        mesh.positions.emplace_back(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z);

        if (assimpMesh->HasTextureCoords(0)) {
            mesh.uvs.emplace_back(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y);
        }
        if (assimpMesh->HasNormals()) {
            mesh.normals.emplace_back(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z);
        }

        if (assimpMesh->HasTangentsAndBitangents()) {
            mesh.tangents.emplace_back(assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z);
            mesh.bitangents.emplace_back(assimpMesh->mBitangents[i].x, assimpMesh->mBitangents[i].y, assimpMesh->mBitangents[i].z);
        }
    }

    // extract indices
    //mesh.indices.reserve(assimpMesh->mNumFaces);
    for (size_t i = 0; i < assimpMesh->mNumFaces; i++) {
        assert(assimpMesh->mFaces[i].mNumIndices == 3);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[0]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[1]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[2]);
    }

    mesh.generateAABB();
    mesh.uploadIndices();
    mesh.uploadVertices();

    mesh.material = materials[assimpMesh->mMaterialIndex];
}

/////////////////////////////////////////////////////////////////////////////////////////

void AssimpImporter::LoadBones(entt::entity entity, const aiMesh* assimpMesh) {
    auto& mesh = scene->Get<ecs::MeshComponent>(entity);
    auto& animation = scene->Add<ecs::MeshAnimationComponent>(entity);

    if (assimpScene->HasAnimations()) {
        animation.animation = Animation(assimpScene->mAnimations[0]);
    } else {
        return;
    }

    // extract bone structure
    // TODO: figure this mess out
    animation.boneWeights.resize(assimpMesh->mNumVertices);
    animation.boneIndices.resize(assimpMesh->mNumVertices);

    for (size_t i = 0; i < assimpMesh->mNumBones; i++) {
        auto bone = assimpMesh->mBones[i];
        int boneIndex = 0;

        if (animation.bonemapping.find(bone->mName.C_Str()) == animation.bonemapping.end()) {
            boneIndex = animation.boneCount;
            animation.boneCount++;
            ecs::BoneInfo bi;
            animation.boneInfos.push_back(bi);
            animation.boneInfos[boneIndex].boneOffset = Assimp::toMat4(bone->mOffsetMatrix);
            animation.bonemapping[bone->mName.C_Str()] = boneIndex;
        } else {
            std::puts("found existing bone in map");
            boneIndex = animation.bonemapping[bone->mName.C_Str()];
        }

        auto addBoneData = [](ecs::MeshAnimationComponent& anim, uint32_t index, uint32_t boneID, float weight) {
            for (int i = 0; i < 4; i++) {
                if (anim.boneWeights[index][i] == 0.0f) {
                    anim.boneIndices[index][i] = boneID;
                    anim.boneWeights[index][i] = weight;
                    return;
                }
            }

            std::puts("Discarding excess bone data..");
        };

        for (size_t j = 0; j < bone->mNumWeights; j++) {
            int vertexID = assimpMesh->mBones[i]->mWeights[j].mVertexId;
            float weight = assimpMesh->mBones[i]->mWeights[j].mWeight;
            addBoneData(animation, vertexID, boneIndex, weight);
        }
    }

    animation.boneTransforms.resize(animation.boneCount);
    for (int i = 0; i < animation.boneCount; i++) {
        animation.boneInfos[i].finalTransformation = glm::mat4(1.0f);
        animation.boneTransforms[i] = animation.boneInfos[i].finalTransformation;
    }

    animation.uploadRenderData(mesh);

    aiNode* rootBone = nullptr;
    std::stack<aiNode*> nodes;
    nodes.push(assimpScene->mRootNode);
    while (!nodes.empty()) {
        if (rootBone) {
            break;
        }

        auto current = nodes.top();
        nodes.pop();

        for (uint32_t b = 0; b < assimpMesh->mNumBones; b++) {
            if (rootBone) {
                break;
            }
            // check if current node is a bone
            if (assimpMesh->mBones[b]->mName == current->mName) {
                // check if its parent is a bone, if not it is the root bone
                bool isRoot = true;
                for (uint32_t parentIndex = 0; parentIndex < assimpMesh->mNumBones; parentIndex++) {
                    if (current->mParent->mName == assimpMesh->mBones[parentIndex]->mName) {
                        isRoot = false;
                    }
                }

                if (isRoot) {
                    rootBone = current;
                }
            }
        }

        for (uint32_t i = 0; i < current->mNumChildren; i++) {
            nodes.push(current->mChildren[i]);
        }
    }

    animation.boneTreeRootNode.name = rootBone->mName.C_Str();

    std::function<void(aiNode* node, ecs::BoneTreeNode& boneNode)> copyBoneNode;
    copyBoneNode = [&](aiNode* node, ecs::BoneTreeNode& boneNode) -> void {
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            auto childNode = node->mChildren[i];
            auto it = animation.bonemapping.find(childNode->mName.C_Str());
            if (it != animation.bonemapping.end()) {
                auto& child = boneNode.children.emplace_back();
                child.name = childNode->mName.C_Str();
                copyBoneNode(childNode, child);
            }
        }
    };

    copyBoneNode(rootBone, animation.boneTreeRootNode);
}

void AssimpImporter::LoadMaterial(entt::entity entity, const aiMaterial* assimpMaterial) {
    auto& material = scene->Add<ecs::MaterialComponent>(entity);

    aiString albedoFile, normalmapFile, metalroughFile;
    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
    assimpMaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

    aiColor4D diffuse;
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
        material.baseColour = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };
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
        auto relativePath = std::filesystem::relative(directory.string() + albedoFile.C_Str(), ec).string();
        auto assetPath = TextureAsset::create(relativePath);
        material.albedoFile = assetPath;
    }
    if (normalmapFile.length) {
        auto relativePath = std::filesystem::relative(directory.string() + normalmapFile.C_Str(), ec).string();
        auto assetPath = TextureAsset::create(relativePath);
        material.normalFile = assetPath;
    }
    if (metalroughFile.length) {
        auto relativePath = std::filesystem::relative(directory.string() + metalroughFile.C_Str(), ec).string();
        auto assetPath = TextureAsset::create(relativePath);
        material.mrFile = assetPath;
    }
}

} // raekor