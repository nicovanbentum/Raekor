#include "pch.h"
#include "VKScene.h"

#include "VKUtil.h"
#include "VKDevice.h"

#include "buffer.h"

namespace Raekor::VK {

void VKScene::load(Device& device) {
    constexpr unsigned int flags =
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_PreTransformVertices |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_Debone |
        aiProcess_ValidateDataStructure;

    Assimp::Importer importer;
    
    std::string path = "resources/models/Sponza/Sponza.gltf";
    const auto scene = importer.ReadFile(path, flags);
    
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    meshes.resize(scene->mNumMeshes);
    std::vector<Vertex> vertices;
    std::vector<Triangle> indices;

    for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
        auto assimpMesh = scene->mMeshes[m];

        VKMesh& mesh = meshes[m];
        mesh.indexOffset = uint32_t(indices.size() * 3);
        mesh.indexRange = assimpMesh->mNumFaces * 3;
        mesh.vertexOffset = uint32_t(vertices.size());

        for (size_t i = 0; i < assimpMesh->mNumVertices; i++) {
            Vertex& v = vertices.emplace_back();
            
            v.pos = { assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z };
            
            if (assimpMesh->HasTextureCoords(0)) {
                v.uv = { assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y };
            }
            
            if (assimpMesh->HasNormals()) {
                v.normal = { assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z };
            }
        }

        // extract indices
        for (size_t i = 0; i < assimpMesh->mNumFaces; i++) {
            m_assert((assimpMesh->mFaces[i].mNumIndices == 3), "Faces require 3 indices");
            
            indices.push_back({ 
                assimpMesh->mFaces[i].mIndices[0], 
                assimpMesh->mFaces[i].mIndices[1], 
                assimpMesh->mFaces[i].mIndices[2] 
            });
        }

    }

    // vertex buffer upload
    const size_t vertexBufferSize = sizeof(Vertex) * vertices.size();
    
    auto [vertexStageBuffer, vertexStageAlloc] = device.createBuffer(
        vertexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    memcpy(device.getMappedPointer(vertexStageAlloc), vertices.data(), vertexBufferSize);

    std::tie(vertexBuffer, vertexBufferAlloc) = device.createBuffer(
        vertexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    device.copyBuffer(vertexStageBuffer, vertexBuffer, vertexBufferSize);
        
    device.destroyBuffer(vertexStageBuffer, vertexStageAlloc);

    // index buffer upload
    const size_t indexBufferSize = sizeof(Triangle) * indices.size();

    auto [indexStageBuffer, indexStageAlloc] = device.createBuffer(
        indexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    memcpy(device.getMappedPointer(indexStageAlloc), indices.data(), indexBufferSize);

    std::tie(indexBuffer, indexBufferAlloc) = device.createBuffer(
        indexBufferSize, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    device.copyBuffer(indexStageBuffer, indexBuffer, indexBufferSize);

    device.destroyBuffer(indexStageBuffer, indexStageAlloc);

    std::map<std::string, int> seen;

    for (unsigned int meshIndex = 0, textureIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        auto assimpMesh = scene->mMeshes[meshIndex];

        std::string texture_path;
        auto material = scene->mMaterials[assimpMesh->mMaterialIndex];
        
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString filename;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
            texture_path = (std::filesystem::path(path).parent_path() / filename.C_Str()).string();
        }

        if (!texture_path.empty()) {
            if (seen.find(texture_path) == seen.end()) {
                meshes[meshIndex].textureIndex = textureIndex;
                seen[texture_path] = textureIndex;
                textureIndex++;
            } else {
                meshes[meshIndex].textureIndex = seen[texture_path];
            }
        } else {
            meshes[meshIndex].textureIndex = -1;
        }
    }


    std::vector<Stb::Image> images = std::vector<Stb::Image>(seen.size());
    for (auto& image : images) {
        image.format = RGBA;
    }

    std::for_each(std::execution::par_unseq, seen.begin(), seen.end(), [&](const std::pair<std::string, int>& kv) {
        images[kv.second].load(kv.first, true);
    });

    textures.reserve(images.size());
    for (const auto& image : images) {
        textures.emplace_back(device, image);
    }
}

} // raekor