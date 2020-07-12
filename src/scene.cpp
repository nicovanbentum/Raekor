#include "pch.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {

ECS::Entity Scene::createObject(const std::string& name) {
    ECS::Entity entity = ECS::newEntity();
    nodes.create(entity);
    names.create(entity).name = name;
    transforms.create(entity);
    entities.push_back(entity);
    return entity;
}

ECS::Entity Scene::createPointLight(const std::string& name) {
    auto entity = createObject(name);
    pointLights.create(entity);
    return entity;
}

ECS::Entity Scene::createDirectionalLight(const std::string& name) {
    auto entity = createObject(name);
    directionalLights.create(entity);
    return entity;
}

ECS::MeshComponent& Scene::addMesh() {
    ECS::Entity entity = createObject("Mesh");
    return meshes.create(entity);
}

void Scene::remove(ECS::Entity entity) {
    bool isNode = nodes.contains(entity);

    nodes.remove(entity);
    names.remove(entity);
    transforms.remove(entity);
    meshes.remove(entity);
    materials.remove(entity);
    pointLights.remove(entity);
    directionalLights.remove(entity);
    animations.remove(entity);

    if (isNode) {
        for (int i = 0; i < nodes.getCount(); /* update on no remove */) {
            if (nodes[i].parent == entity) {
                remove(nodes.getEntity(i));
            } else {
                i += 1;
            }
        }
    }

    // update children boolean for all nodes
    for (int i = 0; i < nodes.getCount(); i++) {
        nodes[i].hasChildren = false;
        for (int j = 0; j < nodes.getCount(); j++) {
            if (nodes[j].parent == nodes.getEntity(i)) {
                nodes[i].hasChildren = true;
            }
        }
    }
}

static glm::mat4 aiMat4toGLM(const aiMatrix4x4& from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
};

void AssimpImporter::loadFromDisk(Scene& scene, const std::string& file, AsyncDispatcher& dispatcher) {
    constexpr unsigned int flags =
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_ValidateDataStructure;

    // the importer takes care of deleting the scene
    const aiScene* assimpScene = importer->ReadFile(file, flags);

    if (!assimpScene) {
        std::clog << "Error loading " << file << ": " << importer->GetErrorString() << '\n';
        return;
    }

    if (!assimpScene->HasMeshes() && !assimpScene->HasMaterials()) {
        return;
    }

    // load all textures into an unordered map
    std::string textureDirectory = parseFilepath(file, PATH_OPTIONS::DIR);
    loadTexturesAsync(assimpScene, textureDirectory, dispatcher);
    
    // recursively process the ai scene graph
    auto rootEntity = scene.createObject(parseFilepath(file, PATH_OPTIONS::FILENAME));
    auto node = scene.nodes.getComponent(rootEntity);
    node->hasChildren = true;
    processAiNode(scene, assimpScene, assimpScene->mRootNode, rootEntity);
}

void AssimpImporter::processAiNode(Scene& scene, const aiScene* aiscene, aiNode* node, ECS::Entity root) {
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

void AssimpImporter::loadMesh(const aiScene* aiscene, Scene& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform, ECS::Entity root) {
    // create new entity and attach components
    ECS::Entity entity = ECS::newEntity();
    ECS::NodeComponent& node = scene.nodes.create(entity);
    ECS::NameComponent& name = scene.names.create(entity);
    ECS::TransformComponent& transform = scene.transforms.create(entity);
    ECS::MeshComponent& mesh = scene.meshes.create(entity);

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
        auto& animation = scene.animations.create(entity);

        animation.boneWeights.resize(mesh.vertices.size());
        animation.boneIndices.resize(mesh.vertices.size());

        for (size_t i = 0; i < assimpMesh->mNumBones; i++) {
            auto bone = assimpMesh->mBones[i];
            int boneIndex = 0;

            if (animation.bonemapping.find(bone->mName.C_Str()) == animation.bonemapping.end()) {
                boneIndex = animation.boneCount;
                animation.boneCount++;
                ECS::BoneInfo bi;
                animation.boneInfos.push_back(bi);
                animation.boneInfos[boneIndex].boneOffset = aiMat4toGLM(bone->mOffsetMatrix);
                animation.bonemapping[bone->mName.C_Str()] = boneIndex;
            } else {
                std::puts("found existing bone in map");
                boneIndex = animation.bonemapping[bone->mName.C_Str()];
            }

            auto addBoneData = [](ECS::MeshAnimationComponent& anim, uint32_t index, uint32_t boneID, float weight) {
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
        
        animation.scene = aiscene;
        animation.uploadRenderData(mesh);
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

    ECS::MaterialComponent& material = scene.materials.create(entity);

    auto defaultNormal = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);

    auto albedoEntry = images.find(albedoFile.C_Str());
    material.albedo = std::make_unique<glTexture2D>();
    material.albedo->bind();

    if (albedoEntry != images.end()) {
        Stb::Image& image = albedoEntry->second;
        material.albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
        material.albedo->setFilter(Sampling::Filter::Trilinear);
        material.albedo->genMipMaps();
    }
    else {
        aiColor4D diffuse; // if the mesh doesn't have an albedo on disk we create a single pixel texture with its diffuse colour
        if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
            material.albedo->init(1, 1, { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT }, &diffuse[0]);
            material.albedo->setFilter(Sampling::Filter::None);
            material.albedo->setWrap(Sampling::Wrap::Repeat);
        }
    }

    auto normalsEntry = images.find(normalmapFile.C_Str());
    material.normals = std::make_unique<glTexture2D>();
    material.normals->bind();

    if (normalsEntry != images.end()) {
        Stb::Image& image = normalsEntry->second;
        material.normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
        material.normals->setFilter(Sampling::Filter::Trilinear);
        material.normals->genMipMaps();
    } else {
        constexpr auto tbnAxis = glm::vec<4, float>(0.5f, 0.5f, 1.0f, 1.0f);
        material.normals->init(1, 1, { GL_RGBA16F, GL_RGBA, GL_FLOAT }, glm::value_ptr(tbnAxis));
        material.normals->setFilter(Sampling::Filter::None);
        material.normals->setWrap(Sampling::Wrap::Repeat);
    }

    auto metalroughEntry = images.find(metalroughFile.C_Str());

    if (metalroughEntry != images.end()) {
        Stb::Image& image = metalroughEntry->second;
        material.metalrough = std::make_unique<glTexture2D>();
        material.metalrough->bind();
        material.metalrough->init(image.w, image.h, { GL_RGBA16F, GL_RGBA, GL_UNSIGNED_BYTE }, image.pixels);
        material.metalrough->setFilter(Sampling::Filter::None);
        material.metalrough->setWrap(Sampling::Wrap::ClampEdge);
    }
}

void AssimpImporter::loadTexturesAsync(const aiScene* scene, const std::string& directory, AsyncDispatcher& dispatcher) {
    for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
        m_assert(scene && scene->HasMeshes(), "failed to load mesh");
        auto aiMesh = scene->mMeshes[index];

        aiString albedoFile, normalmapFile, metalroughFile;
        auto material = scene->mMaterials[aiMesh->mMaterialIndex];
        material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
        material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

        if (strcmp(albedoFile.C_Str(), "") != 0) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = true;
            image.filepath = directory + std::string(albedoFile.C_Str());
            images[albedoFile.C_Str()] = image;
        }


        if (strcmp(normalmapFile.C_Str(), "") != 0) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = false;
            image.filepath = directory + std::string(normalmapFile.C_Str());
            images[normalmapFile.C_Str()] = image;
        }


        if (strcmp(metalroughFile.C_Str(), "") != 0) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = false;
            image.filepath = directory + std::string(metalroughFile.C_Str());
            images[metalroughFile.C_Str()] = image;
        }
    }

    // asyncronously load textures from disk
    for (auto& pair : images) {
        dispatcher.dispatch([&pair]() {
            pair.second.load(pair.second.filepath, true);
        });
    }

    dispatcher.wait();
}

void updateTransforms(Scene& scene) {
    for (int i = 0; i < scene.nodes.getCount(); i++) {
        auto node = scene.nodes[i];

        // update the world matrix by multiplying the local matrix with each parent matrix
        auto transform = scene.transforms[i].matrix;
        for (auto parent = node.parent; parent != NULL; parent = scene.nodes.getComponent(parent)->parent) {
            transform *= scene.transforms.getComponent(parent)->matrix;
        }

        scene.transforms[i].worldTransform = transform;
    }
}

ECS::Entity pickObject(Scene& scene, Math::Ray& ray) {
    ECS::Entity pickedEntity = NULL;
    std::map<float, ECS::Entity> boxesHit;

    for (size_t i = 0; i < scene.meshes.getCount(); i++) {
        ECS::Entity entity = scene.meshes.getEntity(i);

        ECS::MeshComponent& mesh = scene.meshes[i];

        // get the OBB transformation matrix
        ECS::TransformComponent* transform = scene.transforms.getComponent(entity);
        const glm::mat4& worldTransform = transform ? transform->worldTransform : glm::mat4(1.0f);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // check for ray hit
        auto scaledMin = mesh.aabb[0] * transform->scale;
        auto scaledMax = mesh.aabb[1] * transform->scale;
        std::cout << glm::to_string(worldTransform) << std::endl;
        std::cout << "min " << glm::to_string(scaledMin) << ", max " << glm::to_string(scaledMax) << std::endl;
        auto hitResult = ray.hitsOBB(scaledMin, scaledMax, worldTransform);

        if (hitResult.has_value()) {
            boxesHit[hitResult.value()] = entity;
        }
    }

    for (auto& pair : boxesHit) {
        auto mesh = scene.meshes.getComponent(pair.second);
        auto transform = scene.transforms.getComponent(pair.second);

        std::cout << "Hit " << pair.second << " at distance " << pair.first << std::endl;
        for (auto& triangle : mesh->indices) {
            auto v0 = glm::vec3(transform->worldTransform * glm::vec4(mesh->vertices[triangle.p1].pos, 1.0));
            auto v1 = glm::vec3(transform->worldTransform * glm::vec4(mesh->vertices[triangle.p2].pos, 1.0));
            auto v2 = glm::vec3(transform->worldTransform * glm::vec4(mesh->vertices[triangle.p3].pos, 1.0));

            auto triangleHitResult = ray.hitsTriangle(v0, v1, v2);
            if (triangleHitResult.has_value()) {
                pickedEntity = pair.second;
                std::cout << "Picking " << pickedEntity << std::endl;
                return pickedEntity;
            }
        }
    }

    return pickedEntity;
}

} // Namespace Raekor