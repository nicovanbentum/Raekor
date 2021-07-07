#include "pch.h"
#include "VKScene.h"

namespace Raekor::VK {

void VKScene::load(Context& context) {
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

    std::map<std::string, int> seen;

    meshes.reserve(scene->mNumMeshes);
    std::vector<Vertex> vertices;
    std::vector<Triangle> indices;

    for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
        auto ai_mesh = scene->mMeshes[m];

        VKMesh mm;
        mm.indexOffset = static_cast<uint32_t>(indices.size() * 3);
        mm.indexRange = ai_mesh->mNumFaces * 3;
        mm.vertexOffset = static_cast<uint32_t>(vertices.size());
        meshes.push_back(mm);

        for (size_t i = 0; i < ai_mesh->mNumVertices; i++) {
            Vertex v;
            v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
            if (ai_mesh->HasTextureCoords(0)) {
                v.uv = { ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
            }
            if (ai_mesh->HasNormals()) {
                v.normal = { ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
            }
            vertices.push_back(std::move(v));
        }
        // extract indices
        for (size_t i = 0; i < ai_mesh->mNumFaces; i++) {
            m_assert((ai_mesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
            indices.push_back({ ai_mesh->mFaces[i].mIndices[0], ai_mesh->mFaces[i].mIndices[1], ai_mesh->mFaces[i].mIndices[2] });
        }

    }

    {   // vertex buffer upload
        const size_t sizeInBytes = sizeof(Vertex) * vertices.size();
        auto [stagingBuffer, stagingAlloc, stagingBufferAllocInfo] = context.device.createStagingBuffer(sizeInBytes);

        // copy the data over
        memcpy(stagingBufferAllocInfo.pMappedData, vertices.data(), sizeInBytes);

        VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        vbInfo.size = sizeInBytes;
        vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = NULL;

        auto vkresult = vmaCreateBuffer(context.device.getAllocator(), &vbInfo, &allocInfo, &vertexBuffer, &vertexBufferAlloc, &vertexBufferAllocInfo);
        assert(vkresult == VK_SUCCESS);

        context.device.copyBuffer(stagingBuffer, vertexBuffer, sizeInBytes);

        vmaDestroyBuffer(context.device.getAllocator(), stagingBuffer, stagingAlloc);
    }

    {   // index buffer upload
        const size_t sizeInBytes = sizeof(Triangle) * indices.size();
        auto [stagingBuffer, stagingAlloc, stagingBufferAllocInfo] = context.device.createStagingBuffer(sizeInBytes);

        // copy the data over
        memcpy(stagingBufferAllocInfo.pMappedData, indices.data(), sizeInBytes);

        VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        vbInfo.size = sizeInBytes;
        vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = NULL;

        auto vkresult = vmaCreateBuffer(context.device.getAllocator(), &vbInfo, &allocInfo, &indexBuffer, &indexBufferAlloc, &indexBufferAllocInfo);
        assert(vkresult == VK_SUCCESS);

        context.device.copyBuffer(stagingBuffer, indexBuffer, sizeInBytes);

        vmaDestroyBuffer(context.device.getAllocator(), stagingBuffer, stagingAlloc);
    }

    for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
        auto ai_mesh = scene->mMeshes[m];

        std::string texture_path;
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString filename;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
            texture_path = (std::filesystem::path(path).parent_path() / filename.C_Str()).string();
        }

        if (!texture_path.empty()) {
            if (seen.find(texture_path) == seen.end()) {
                meshes[m].textureIndex = ti;
                seen[texture_path] = ti;
                ti++;
            } else meshes[m].textureIndex = seen[texture_path];
        } else meshes[m].textureIndex = -1;
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
        textures.emplace_back(context, image, context.device.getAllocator());
    }
}

} // raekor