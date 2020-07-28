#include "pch.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {

static glm::mat4 aiMat4toGLM(const aiMatrix4x4& from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
};

void AssimpImporter::loadFromDisk(entt::registry& scene, const std::string& file, AsyncDispatcher& dispatcher) {
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
    auto assimpScene = importer->ReadFile(file, flags);

    if (!assimpScene) {
        std::clog << "Error loading " << file << ": " << importer->GetErrorString() << '\n';
        return;
    }

    if (!assimpScene->HasMeshes() && !assimpScene->HasMaterials()) {
        return;
    }

    // recursively process the ai scene graph
    auto rootEntity = scene.create();
    scene.emplace<ecs::NameComponent>(rootEntity, parseFilepath(file, PATH_OPTIONS::FILENAME));
    scene.emplace<ecs::TransformComponent>(rootEntity);
    auto& node = scene.emplace<ecs::NodeComponent>(rootEntity);
    node.hasChildren = true;
    processAiNode(scene, assimpScene, assimpScene->mRootNode, rootEntity);

    // async asset loading
    loadAssetsFromDisk(scene, dispatcher, parseFilepath(file, PATH_OPTIONS::DIR));
}

void AssimpImporter::processAiNode(entt::registry& scene, const aiScene* aiscene, aiNode* node, entt::entity root) {
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = aiscene->mMeshes[node->mMeshes[i]];
        aiMaterial* material = aiscene->mMaterials[mesh->mMaterialIndex];
        // load mesh with material and transform into raekor scene
        loadMesh(aiscene, scene, mesh, material, node->mTransformation, root);
    }
    // recursive node processing
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        processAiNode(scene, aiscene, node->mChildren[i], root);
    }
}

void AssimpImporter::loadMesh(const aiScene* aiscene, entt::registry& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform, entt::entity root) {
    // create new entity and attach components
    auto entity = scene.create();
    auto& node = scene.emplace<ecs::NodeComponent>(entity);
    auto& name = scene.emplace<ecs::NameComponent>(entity);
    auto& transform = scene.emplace<ecs::TransformComponent>(entity);
    auto& mesh = scene.emplace<ecs::MeshComponent>(entity);


    name.name = assimpMesh->mName.C_Str();
    node.parent = root;

    aiVector3D position, scale, rotation;
    localTransform.Decompose(scale, rotation, position);

    transform.position = { position.x, position.y, position.z };
    transform.scale = { scale.x, scale.y, scale.z };
    transform.rotation = { glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z) };
    transform.recalculateMatrix();

    // extract vertices
    mesh.vertices.reserve(assimpMesh->mNumVertices);
    for (size_t i = 0; i < mesh.vertices.capacity(); i++) {
        Vertex v = {};
        v.pos = { assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z };

        if (assimpMesh->HasTextureCoords(0)) {
            v.uv = { assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y };
        }
        if (assimpMesh->HasNormals()) {
            v.normal = { assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z };
        }

        if (assimpMesh->HasTangentsAndBitangents()) {
            v.tangent = { assimpMesh->mTangents[i].x, assimpMesh->mTangents[i].y, assimpMesh->mTangents[i].z };
            v.binormal = { assimpMesh->mBitangents[i].x, assimpMesh->mBitangents[i].y, assimpMesh->mBitangents[i].z };
        }

        mesh.vertices.push_back(std::move(v));
    }

    if (assimpMesh->HasBones()) {
        auto& animation = scene.emplace<ecs::MeshAnimationComponent>(entity);

        animation.animation = Animation(aiscene->mAnimations[0]);

        // extract bone structure
        // TODO: figure this mess out
        animation.boneWeights.resize(mesh.vertices.size());
        animation.boneIndices.resize(mesh.vertices.size());

        for (size_t i = 0; i < assimpMesh->mNumBones; i++) {
            auto bone = assimpMesh->mBones[i];
            int boneIndex = 0;

            if (animation.bonemapping.find(bone->mName.C_Str()) == animation.bonemapping.end()) {
                boneIndex = animation.boneCount;
                animation.boneCount++;
                ecs::BoneInfo bi;
                animation.boneInfos.push_back(bi);
                animation.boneInfos[boneIndex].boneOffset = aiMat4toGLM(bone->mOffsetMatrix);
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
        nodes.push(aiscene->mRootNode);
        while (!nodes.empty()) {
            if (rootBone) break;

            auto current = nodes.top();
            nodes.pop();
            
            for (uint32_t b = 0; b < assimpMesh->mNumBones; b++) {
                if (rootBone) break;
                // check if current node is a bone
                if (assimpMesh->mBones[b]->mName == current->mName) {
                    // check if its parent is a bone, if not it is the root bone
                    bool isRoot = true;
                    for (uint32_t parentIndex = 0; parentIndex < assimpMesh->mNumBones; parentIndex++) {
                        if (current->mParent->mName == assimpMesh->mBones[parentIndex]->mName) {
                            isRoot = false;
                        }
                    }

                    if (isRoot) rootBone = current;
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

    // extract indices
    mesh.indices.reserve(assimpMesh->mNumFaces);
    for (size_t i = 0; i < mesh.indices.capacity(); i++) {
        m_assert((assimpMesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
        mesh.indices.emplace_back(assimpMesh->mFaces[i].mIndices[0], assimpMesh->mFaces[i].mIndices[1], assimpMesh->mFaces[i].mIndices[2]);
    }

    mesh.generateAABB();
    transform.localPosition = (mesh.aabb[0] + mesh.aabb[1]) / 2.0f;

    // upload the mesh buffers to the GPU
    mesh.uploadVertices();
    mesh.uploadIndices();


    // get material textures from Assimp's import
    aiString albedoFile, normalmapFile, metalroughFile;

    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
    assimpMaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

    auto& material = scene.emplace<ecs::MaterialComponent>(entity);
    aiColor4D diffuse; 
    if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
        material.baseColour = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };
    }

    material.albedoFile = albedoFile.C_Str();
    material.normalFile = normalmapFile.C_Str();
    material.mrFile = metalroughFile.C_Str();
}

void destroyNode(entt::registry& scene, entt::entity entity) {
    if (!scene.has<ecs::NodeComponent>(entity)) return;
    auto view = scene.view<ecs::NodeComponent>();
    std::unordered_set<entt::entity> destructible;
    // loop over every node in the scene
    for (auto e : view) {
        std::vector<entt::entity> parents;
        // loop over the node's parent chain, storing parents along the way
        for (auto parent = view.get<ecs::NodeComponent>(e).parent; parent != entt::null; parent = view.get<ecs::NodeComponent>(parent).parent) {
            // if we find the deleted node along the parent chain, we traverse the chain and mark the chain destructable
            if (parent == entity) {
                destructible.insert(e);
                for (auto p : parents) {
                    destructible.insert(p);
                }
                break;
            // else we add the parent to the chain
            } else {
                parents.push_back(parent);
            }
        }
    }

    // destroy all the entities
    for (auto e : destructible) {
        scene.destroy(e);
    }

    scene.destroy(entity);
}

void updateTransforms(entt::registry& scene) {
    auto nodeView = scene.view<ecs::NodeComponent, ecs::TransformComponent>();
    for (auto entity : nodeView) {
        auto& node = scene.get<ecs::NodeComponent>(entity);
        auto& transform = scene.get<ecs::TransformComponent>(entity);
        auto currentMatrix = transform.matrix;

        for (auto parent = node.parent; parent != entt::null; parent = scene.get<ecs::NodeComponent>(parent).parent) {
            currentMatrix *= scene.get<ecs::TransformComponent>(parent).matrix;
        }

        transform.worldTransform = currentMatrix;
    }
}

entt::entity pickObject(entt::registry& scene, Math::Ray& ray) {
    entt::entity pickedEntity = entt::null;
    std::map<float, entt::entity> boxesHit;

    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();
    for (auto entity : view) {
        auto& mesh = view.get<ecs::MeshComponent>(entity);
        auto& transform = view.get<ecs::TransformComponent>(entity);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // check for ray hit
        auto scaledMin = mesh.aabb[0] * transform.scale;
        auto scaledMax = mesh.aabb[1] * transform.scale;
        auto hitResult = ray.hitsOBB(scaledMin, scaledMax, transform.worldTransform);

        if (hitResult.has_value()) {
            boxesHit[hitResult.value()] = entity;
        }
    }

    for (auto& pair : boxesHit) {
        auto& mesh = scene.get<ecs::MeshComponent>(pair.second);
        auto& transform = scene.get<ecs::TransformComponent>(pair.second);

        for (auto& triangle : mesh.indices) {
            auto v0 = glm::vec3(transform.worldTransform * glm::vec4(mesh.vertices[triangle.p1].pos, 1.0));
            auto v1 = glm::vec3(transform.worldTransform * glm::vec4(mesh.vertices[triangle.p2].pos, 1.0));
            auto v2 = glm::vec3(transform.worldTransform * glm::vec4(mesh.vertices[triangle.p3].pos, 1.0));

            auto triangleHitResult = ray.hitsTriangle(v0, v1, v2);
            if (triangleHitResult.has_value()) {
                pickedEntity = pair.second;
                return pickedEntity;
            }
        }
    }

    return pickedEntity;
}

void loadAssetsFromDisk(entt::registry& scene, AsyncDispatcher& dispatcher, const std::string& directory) {
    std::unordered_map<std::string, Stb::Image> images;

    // setup a centralized data structure for all the images
    auto view = scene.view<ecs::MaterialComponent>();
    for (auto entity : view) {
        auto& material = view.get<ecs::MaterialComponent>(entity);

        if (material.albedo) continue;

        if (!directory.empty()) {
            if(!material.albedoFile.empty()) 
                material.albedoFile = directory + material.albedoFile;
            if(!material.normalFile.empty())
                material.normalFile = directory + material.normalFile;
            if(!material.mrFile.empty())
                material.mrFile = directory + material.mrFile;
        }

        images[material.albedoFile] = Stb::Image(RGBA, material.albedoFile);
        images[material.normalFile] = Stb::Image(RGBA, material.normalFile);
        images[material.mrFile] = Stb::Image(RGBA, material.mrFile);
    }

    // load every texture from disk in parallel
    for (auto& pair : images) {
        dispatcher.dispatch([&pair]() {
            pair.second.load(pair.second.filepath, true);
         });
    }

    // wait for disk loading to finish
    dispatcher.wait();

    // upload textures to GPU
    for (auto entity : view) {
        auto& material = view.get<ecs::MaterialComponent>(entity);

        if (material.albedo) continue;

        auto albedoEntry = images.find(material.albedoFile);
        material.albedo = std::make_unique<glTexture2D>();
        material.albedo->bind();

        if (albedoEntry != images.end() && !albedoEntry->first.empty()) {
            Stb::Image& image = albedoEntry->second;
            material.albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
            material.albedo->setFilter(Sampling::Filter::Trilinear);
            material.albedo->genMipMaps();
        }
        else {
                material.albedo->init(1, 1, { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT }, glm::value_ptr(material.baseColour));
                material.albedo->setFilter(Sampling::Filter::None);
                material.albedo->setWrap(Sampling::Wrap::Repeat);
        }

        auto normalsEntry = images.find(material.normalFile);
        material.normals = std::make_unique<glTexture2D>();
        material.normals->bind();

        if (normalsEntry != images.end() && !normalsEntry->first.empty()) {
            Stb::Image& image = normalsEntry->second;
            material.normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
            material.normals->setFilter(Sampling::Filter::Trilinear);
            material.normals->genMipMaps();
        }
        else {
            constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
            material.normals->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(tbnAxis));
            material.normals->setFilter(Sampling::Filter::None);
            material.normals->setWrap(Sampling::Wrap::Repeat);
        }

        auto metalroughEntry = images.find(material.mrFile);
        material.metalrough = std::make_unique<glTexture2D>();

        if (metalroughEntry != images.end() && !metalroughEntry->first.empty()) {
            Stb::Image& image = metalroughEntry->second;
            material.metalrough->bind();
            material.metalrough->init(image.w, image.h, { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE }, image.pixels);
            material.metalrough->setFilter(Sampling::Filter::None);
            material.normals->genMipMaps();
        } else {
            auto metalRoughnessValue = glm::vec4(material.metallic, material.roughness, 0.0f, 1.0f);
            material.metalrough->bind();
            material.metalrough->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT}, glm::value_ptr(metalRoughnessValue));
            material.metalrough->setFilter(Sampling::Filter::None);
            material.metalrough->setWrap(Sampling::Wrap::Repeat);
        }
    }

}

} // Namespace Raekor