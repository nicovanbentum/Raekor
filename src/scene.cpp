#include "pch.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"
#include "serial.h"
#include "systems.h"

namespace Raekor {

Scene::Scene() {
    registry.on_destroy<ecs::MeshComponent>().connect<entt::invoke<&ecs::MeshComponent::destroy>>();
    registry.on_destroy<ecs::MaterialComponent>().connect<entt::invoke<&ecs::MaterialComponent::destroy>>();
    registry.on_destroy<ecs::MeshAnimationComponent>().connect<entt::invoke<&ecs::MeshAnimationComponent::destroy>>();
}

/////////////////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
    registry.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::createObject(const std::string& name) {
    auto entity = registry.create();
    registry.emplace<ecs::NameComponent>(entity, name);
    registry.emplace<ecs::NodeComponent>(entity);
    registry.emplace<ecs::TransformComponent>(entity);
    return entity;
}

/////////////////////////////////////////////////////////////////////////////////////////

entt::entity Scene::pickObject(Math::Ray& ray) {
    entt::entity pickedEntity = entt::null;
    std::map<float, entt::entity> boxesHit;

    auto view = registry.view<ecs::MeshComponent, ecs::TransformComponent>();
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
        auto& mesh = view.get<ecs::MeshComponent>(pair.second);
        auto& transform = view.get<ecs::TransformComponent>(pair.second);

        for (unsigned int i = 0; i < mesh.indices.size(); i += 3) {
            auto v0 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i]], 1.0));
            auto v1 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
            auto v2 = glm::vec3(transform.worldTransform * glm::vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

            auto triangleHitResult = ray.hitsTriangle(v0, v1, v2);
            if (triangleHitResult.has_value()) {
                pickedEntity = pair.second;
                return pickedEntity;
            }
        }
    }

    return pickedEntity;
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::updateTransforms() {
    auto nodeView = registry.view<ecs::NodeComponent, ecs::TransformComponent>();
    for (auto entity : nodeView) {
        auto& node = nodeView.get<ecs::NodeComponent>(entity);
        auto& transform = nodeView.get<ecs::TransformComponent>(entity);
        auto currentMatrix = transform.matrix;

        for (auto parent = node.parent; parent != entt::null; parent = nodeView.get<ecs::NodeComponent>(parent).parent) {
            currentMatrix *= nodeView.get<ecs::TransformComponent>(parent).matrix;
        }

        transform.worldTransform = currentMatrix;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::loadMaterialTextures(const std::vector<entt::entity>& materials) {
    // setup a centralized data structure for all the images

    std::unordered_map<std::string, Stb::Image> images;
    for (auto entity : materials) {
        auto& material = registry.get<ecs::MaterialComponent>(entity);
        if (std::filesystem::is_regular_file(material.albedoFile))
            images[material.albedoFile] = Stb::Image(RGBA, material.albedoFile);
        if (std::filesystem::is_regular_file(material.normalFile))
            images[material.normalFile] = Stb::Image(RGBA, material.normalFile);
        if (std::filesystem::is_regular_file(material.mrFile))
            images[material.mrFile] = Stb::Image(RGBA, material.mrFile);
    }

    // load every texture from disk in parallel
    std::for_each(std::execution::par_unseq, images.begin(), images.end(), [](auto& kv) {
        kv.second.load(kv.first, true);
        });

    for (auto entity : materials) {
        auto& material = registry.get<ecs::MaterialComponent>(entity);

        if (images.find(material.albedoFile) != images.end()) {
            material.createAlbedoTexture(images[material.albedoFile]);
        }
        if (images.find(material.normalFile) != images.end()) {
            material.createNormalTexture(images[material.normalFile]);
        }
        if (images.find(material.mrFile) != images.end()) {
            material.createMetalRoughTexture(images[material.mrFile]);
        }

    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::saveToFile(const std::string& file) {
    std::ofstream outstream(file, std::ios::binary);
    cereal::BinaryOutputArchive output(outstream);
    entt::snapshot{ registry }.entities(output).component<
        ecs::NameComponent, ecs::NodeComponent, ecs::TransformComponent,
        ecs::MeshComponent, ecs::MaterialComponent, ecs::PointLightComponent,
        ecs::DirectionalLightComponent>(output);
}

/////////////////////////////////////////////////////////////////////////////////////////

void Scene::openFromFile(const std::string& file) {
    if (!std::filesystem::is_regular_file(file)) return;
    std::ifstream storage(file, std::ios::binary);

    // TODO: lz4 compression
    ////Read file into buffer
    //std::string buffer;
    //storage.seekg(0, std::ios::end);
    //buffer.resize(storage.tellg());
    //storage.seekg(0, std::ios::beg);
    //storage.read(&buffer[0], buffer.size());

    //char* const regen_buffer = (char*)malloc(buffer.size());
    //const int decompressed_size = LZ4_decompress_safe(buffer.data(), regen_buffer, buffer.size(), bound_size);

    registry.clear();
    
    cereal::BinaryInputArchive input(storage);
    entt::snapshot_loader{ registry }.entities(input).component<
        ecs::NameComponent, ecs::NodeComponent, ecs::TransformComponent,
        ecs::MeshComponent, ecs::MaterialComponent, ecs::PointLightComponent,
        ecs::DirectionalLightComponent>(input);

    // init material render data
    auto materials = registry.view<ecs::MaterialComponent>();
    auto materialEntities = std::vector<entt::entity>();
    materialEntities.assign(materials.data(), materials.data() + materials.size());
    loadMaterialTextures(materialEntities);
    
    // init mesh render data
    auto view = registry.view<ecs::MeshComponent>();
    for (auto entity : view) {
        auto& mesh = view.get<ecs::MeshComponent>(entity);
        mesh.generateAABB();
        mesh.uploadVertices();
        mesh.uploadIndices();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

static glm::mat4 aiMat4toGLM(const aiMatrix4x4& from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
};

/////////////////////////////////////////////////////////////////////////////////////////

bool AssimpImporter::loadFile(Scene& scene, const std::string& file) {
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

    std::cout << importer->GetErrorString() << std::endl;

    auto path = std::filesystem::path(file);
    auto directory = path.parent_path() / "";
    auto filename = path.filename();

    if (!assimpScene) {
        std::clog << "Error loading " << file << ": " << importer->GetErrorString() << '\n';
        return false;
    }

    if (!assimpScene->HasMeshes() && !assimpScene->HasMaterials()) {
        return false;
    }

    auto rootEntity = scene->create();
    scene->emplace<ecs::NameComponent>(rootEntity, filename.string());
    scene->emplace<ecs::TransformComponent>(rootEntity);
    scene->emplace<ecs::NodeComponent>(rootEntity);

    auto materials = loadMaterials(scene, assimpScene, directory.string());
    scene.loadMaterialTextures(materials);

    // load meshes and assign materials
    std::vector<entt::entity> meshes(assimpScene->mNumMeshes);
    for (unsigned int i = 0; i < assimpScene->mNumMeshes; i++) {
        auto assimpMesh = assimpScene->mMeshes[i];
        auto entity = scene->create();
        auto& name = scene->emplace<ecs::NameComponent>(entity);
        auto& mesh = scene->emplace<ecs::MeshComponent>(entity);
        name.name = assimpMesh->mName.C_Str();
        convertMesh(mesh, assimpMesh);

        mesh.material = materials[assimpMesh->mMaterialIndex];

        if (assimpMesh->HasBones()) {
            loadBones(scene, assimpScene, assimpMesh, entity);
        }
    }

    // calculate mesh node and transform hierarchy, simple linear tree traversal
    std::stack<aiNode*> nodes;
    nodes.push(assimpScene->mRootNode);
    while (!nodes.empty()) {
        auto assimpNode = nodes.top();
        nodes.pop();
        for (unsigned int i = 0; i < assimpNode->mNumMeshes; i++) {
            // get the associated entity for the mesh
            auto entity = meshes[assimpNode->mMeshes[i]];
            
            // make it a child to the root node
            auto& node = scene->emplace<ecs::NodeComponent>(entity);
            NodeSystem::append(scene, scene->get<ecs::NodeComponent>(rootEntity), node);
            
            auto& transform = scene->emplace<ecs::TransformComponent>(entity);
            auto& mesh = scene->get<ecs::MeshComponent>(entity);

            // translate assimp transformation to glm
            aiVector3D position, scale, rotation;
            auto localTransform = assimpNode->mTransformation;
            localTransform.Decompose(scale, rotation, position);

            transform.position = { position.x, position.y, position.z };
            transform.scale = { scale.x, scale.y, scale.z };
            transform.rotation = { glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z) };
            transform.recalculateMatrix();

            if (entity != rootEntity) {
                node.parent = rootEntity;
            }
        }

        // push children
        for (unsigned int j = 0; j < assimpNode->mNumChildren; j++) {
            nodes.push(assimpNode->mChildren[j]);
        }
    }

    for (auto entity : meshes) {
        auto& mesh = scene->get<ecs::MeshComponent>(entity);
        mesh.uploadVertices();
        mesh.uploadIndices();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool AssimpImporter::convertMesh(ecs::MeshComponent& mesh, aiMesh* assimpMesh) {
    // extract vertices
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
        if (assimpMesh->mFaces[i].mNumIndices != 3) return false;
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[0]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[1]);
        mesh.indices.push_back(assimpMesh->mFaces[i].mIndices[2]);
    }

    mesh.generateAABB();

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

std::vector<entt::entity> AssimpImporter::loadMaterials(entt::registry& scene, const aiScene* aiscene, const std::string& directory) {
    std::vector<entt::entity> materials(aiscene->mNumMaterials);

    for (unsigned int i = 0; i < aiscene->mNumMaterials; i++) {
        auto aiMat = aiscene->mMaterials[i];

        // get material textures from Assimp's import
        aiString albedoFile, normalmapFile, metalroughFile;

        aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        aiMat->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
        aiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

        auto materialEntity = scene.create();
        auto& materialName = scene.emplace<ecs::NameComponent>(materialEntity);
        auto& material = scene.emplace<ecs::MaterialComponent>(materialEntity);
        aiColor4D diffuse;
        if (AI_SUCCESS == aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
            material.baseColour = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };
        }

        std::error_code ec;
        material.albedoFile = std::filesystem::relative(directory + albedoFile.C_Str(), ec).string();
        material.normalFile = std::filesystem::relative(directory + normalmapFile.C_Str(), ec).string();
        material.mrFile = std::filesystem::relative(directory + metalroughFile.C_Str(), ec).string();

        if (strcmp(aiMat->GetName().C_Str(), "") != 0) {
            materialName.name = aiMat->GetName().C_Str();
        } else {
            materialName.name = "Material " + std::to_string(scene.view<ecs::MaterialComponent, ecs::NameComponent>().size());
        }

        materials[i] = materialEntity;
    }

    return materials;
}

/////////////////////////////////////////////////////////////////////////////////////////

void AssimpImporter::loadBones(entt::registry& scene, const aiScene* aiscene, aiMesh* assimpMesh, entt::entity entity) {
    auto& mesh = scene.get<ecs::MeshComponent>(entity);
    
    auto& animation = scene.emplace<ecs::MeshAnimationComponent>(entity);
    animation.animation = Animation(aiscene->mAnimations[0]);

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
            animation.boneInfos[boneIndex].boneOffset = aiMat4toGLM(bone->mOffsetMatrix);
            animation.bonemapping[bone->mName.C_Str()] = boneIndex;
        }
        else {
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
} // Raekor