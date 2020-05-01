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

    void Scene::loadFromFile(const std::string& file) {
        constexpr unsigned int flags =
            aiProcess_GenNormals |
            aiProcess_Triangulate |
            aiProcess_SortByPType |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenUVCoords |
            aiProcess_OptimizeMeshes |
            aiProcess_Debone |
            aiProcess_ValidateDataStructure;

        Assimp::Importer importer;
        auto scene = importer.ReadFile(file, flags);

        if (!scene) {
            std::cout << importer.GetErrorString() << '\n';
            throw std::runtime_error("Failed to load " + file);
        }

        if (!scene->HasMeshes() && !scene->HasMaterials()) {
            return;
        }

        // pre-load all the textures asynchronously
        std::vector<Stb::Image> albedos;
        std::vector<Stb::Image> normals;

        for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
            m_assert(scene && scene->HasMeshes(), "failed to load mesh");
            auto aiMesh = scene->mMeshes[index];

            aiString albedoFile, normalmapFile;
            auto material = scene->mMaterials[aiMesh->mMaterialIndex];
            material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
            material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);


            std::string albedoFilepath = parseFilepath(file, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
            std::string normalmapFilepath = parseFilepath(file, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());

            if (!albedoFilepath.empty()) {
                Stb::Image image;
                image.format = RGBA;
                image.isSRGB = true;
                image.filepath = albedoFilepath;
                albedos.push_back(image);
            }

            if (!normalmapFilepath.empty()) {
                Stb::Image image;
                image.format = RGBA;
                image.isSRGB = false;
                image.filepath = normalmapFilepath;
                normals.push_back(image);
            }
        }

        // asyncronously load textures from disk
        std::vector<std::future<void>> futures;
        for (auto& img : albedos) {
            futures.push_back(std::async(std::launch::async, &Stb::Image::load, &img, img.filepath, true));
        }

        for (auto& img : normals) {
            futures.push_back(std::async(std::launch::async, &Stb::Image::load, &img, img.filepath, true));
        }

        for (auto& future : futures) {
            future.wait();
        }

        auto processMesh = [&](aiMesh* assimpMesh, aiMatrix4x4 localTransform, const aiScene* scene) {
            // create new entity and attach a mesh component
            Entity entity = newEntity();
            MeshComponent& mesh = meshes.create(entity);

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
            auto assimpMaterial = scene->mMaterials[assimpMesh->mMaterialIndex];

            assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE);
            assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
            assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

            std::string albedoPath, normalsPath;
            if (strcmp(albedoFile.C_Str(), "") != 0) {
                albedoPath = parseFilepath(file, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
            }
            if (strcmp(normalmapFile.C_Str(), "") != 0) {
                normalsPath = parseFilepath(file, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());
            }

            auto albedoIter = std::find_if(albedos.begin(), albedos.end(), [&](const Stb::Image& img) {
                return img.filepath == albedoPath;
                });

            auto normalIter = std::find_if(normals.begin(), normals.end(), [&](const Stb::Image& img) {
                return img.filepath == normalsPath;
                });

            // attach material component to the mesh
            // TODO: materials shouldnt be component only, materialcomponent should a contain material asset
            MaterialComponent& material = materials.create(entity);

            // super hacky, if the imported material has an albedo texture we load else 
            // we create a 1x1 texture of it's diffuse texture (assuming it has a diffuse colour)
            // TODO: sort this out
            if (albedoIter != albedos.end()) {
                auto index = albedoIter - albedos.begin();
                auto& image = albedos[index];

                material.albedo.bind();
                material.albedo.init(image.w, image.h, Format::SRGBA_U8, image.pixels);
                material.albedo.setFilter(Sampling::Filter::Trilinear);
                material.albedo.genMipMaps();
                material.albedo.unbind();
            }
            else {
                aiColor4D diffuse;
                if (AI_SUCCESS == aiGetMaterialColor(assimpMaterial, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
                    material.albedo.bind();
                    Format::Format format = { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT };
                    material.albedo.init(1, 1, format, &diffuse[0]);
                    material.albedo.setFilter(Sampling::Filter::None);
                    material.albedo.setWrap(Sampling::Wrap::Repeat);
                    material.albedo.unbind();
                }

            }

            //normal map is loaded either way
            if (normalIter != normals.end()) {
                auto index = normalIter - normals.begin();
                auto& image = normals[index];

                material.normals.bind();
                material.normals.init(image.w, image.h, Format::RGBA_U8, image.pixels);
                material.normals.setFilter(Sampling::Filter::Trilinear);
                material.normals.genMipMaps();
                material.normals.unbind();
            }
        };

        std::function<void(aiNode*, const aiScene*)> processNode = [&](aiNode* node, const aiScene* scene) {
            for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                processMesh(mesh, node->mTransformation, scene);
            }

            for (uint32_t i = 0; i < node->mNumChildren; i++) {
                processNode(node->mChildren[i], scene);
            }
        };
        // process the assimp node tree, creating a scene object for every mesh with its textures and transformation
        processNode(scene->mRootNode, scene);
    }

} // ecs
} // raekor