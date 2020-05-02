#include "pch.h"
#include "ecs.h"

namespace Raekor {
namespace ECS {

    void Scene:: createObject(const char* name) {
        Entity entity = newEntity();
        names.create(entity).name = name;
        entities.push_back(entity);
    }

    void Scene:: attachCube(Entity entity) {
        auto& mesh = meshes.create(entity);
        mesh.init(Shape::Cube);
    }

    void AssimpImporter::loadFromDisk(ECS::Scene& scene, const std::string& file) {
        constexpr unsigned int flags =
            aiProcess_GenNormals |
            aiProcess_Triangulate |
            aiProcess_SortByPType |
            aiProcess_PreTransformVertices |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenUVCoords |
            aiProcess_OptimizeMeshes |
            aiProcess_Debone |
            aiProcess_ValidateDataStructure;

        Assimp::Importer importer;
        auto assimpScene = importer.ReadFile(file, flags);

        if (!assimpScene) {
            std::cout << importer.GetErrorString() << '\n';
            throw std::runtime_error("Failed to load " + file);
        }

        if (!assimpScene->HasMeshes() && !assimpScene->HasMaterials()) {
            return;
        }

        // load all textures into an unordered map
        std::string textureDirectory = parseFilepath(file, PATH_OPTIONS::DIR);
        loadTexturesAsync(assimpScene, textureDirectory);

        // recursively process the ai scene graph
        processAiNode(scene, assimpScene, assimpScene->mRootNode);
    }

    void AssimpImporter::processAiNode(ECS::Scene& scene, const aiScene* aiscene, aiNode* node) {
        for (uint32_t i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = aiscene->mMeshes[node->mMeshes[i]];
            aiMaterial* material = aiscene->mMaterials[mesh->mMaterialIndex];
            // load mesh with material and transform into raekor scene
            loadMesh(scene, mesh, material, node->mTransformation);
        }

        // recursive node processing
        for (uint32_t i = 0; i < node->mNumChildren; i++) {
            processAiNode(scene, aiscene, node->mChildren[i]);
        }
    }

    void AssimpImporter::loadMesh(ECS::Scene& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform) {
        // create new entity and attach a mesh component
        Entity entity = newEntity();
        MeshComponent& mesh = scene.meshes.create(entity);

        NameComponent& name = scene.names.create(entity);
        name.name = assimpMesh->mName.C_Str();

        // TODO: temp
        std::cout << "loading " << assimpMesh->mName.C_Str() << "..." << std::endl;

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
        // extract indices
        mesh.indices.reserve(assimpMesh->mNumFaces);
        for (size_t i = 0; i < mesh.indices.capacity(); i++) {
            m_assert((assimpMesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
            mesh.indices.emplace_back(assimpMesh->mFaces[i].mIndices[0], assimpMesh->mFaces[i].mIndices[1], assimpMesh->mFaces[i].mIndices[2]);
        }

        // get material textures from Assimp's import
        aiString albedoFile, normalmapFile;

        assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE);
        assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

        MaterialComponent& material = scene.materials.create(entity);


        auto albedoEntry = images.find(albedoFile.C_Str());
        if (albedoEntry != images.end()) {
            Stb::Image& image = albedoEntry->second;
            material.albedo = std::make_unique<glTexture2D>();
            material.albedo->bind();
            material.albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
            material.albedo->setFilter(Sampling::Filter::Trilinear);
            material.albedo->genMipMaps();
            material.albedo->unbind();
        }
        else {
            aiColor4D diffuse; // if the mesh doesn't have an albedo on disk we create a single pixel texture with its diffuse colour
            if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
                material.albedo = std::make_unique<glTexture2D>();
                material.albedo->bind();
                Format::Format format = { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT };
                diffuse = aiColor4D(1.0, 0.0, 0.0, 1.0);
                material.albedo->init(1, 1, format, &diffuse[0]);
                material.albedo->setFilter(Sampling::Filter::None);
                material.albedo->setWrap(Sampling::Wrap::Repeat);
                material.albedo->unbind();
            }
        }

        auto normalsEntry = images.find(normalmapFile.C_Str());
        if (normalsEntry != images.end()) {
            Stb::Image& image = normalsEntry->second;
            material.normals = std::make_unique<glTexture2D>();
            material.normals->bind();
            material.normals->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
            material.normals->setFilter(Sampling::Filter::Trilinear);
            material.normals->genMipMaps();
            material.normals->unbind();
        }
    }

    void AssimpImporter::loadTexturesAsync(const aiScene* scene, const std::string& directory) {
        for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
            m_assert(scene && scene->HasMeshes(), "failed to load mesh");
            auto aiMesh = scene->mMeshes[index];

            aiString albedoFile, normalmapFile;
            auto material = scene->mMaterials[aiMesh->mMaterialIndex];
            material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
            material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

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
        }

        // asyncronously load textures from disk
        std::vector<std::future<void>> futures;
        for (auto& pair : images) {
            futures.push_back(std::async(std::launch::async, &Stb::Image::load, &pair.second, pair.second.filepath, true));
        }

        for (auto& future : futures) {
            future.wait();
        }
    }
} // ecs
} // raekor