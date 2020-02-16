#include "pch.h"
#include "model.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {

Model::Model(const std::string& m_file) : path(m_file) {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);

    if (!path.empty()) {
        load_from_disk();
    }
}

static std::mutex mutex;

void Model::load_mesh(uint64_t index) {
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    auto ai_mesh = scene->mMeshes[index];
    m_assert(ai_mesh->HasPositions() && ai_mesh->HasNormals(), "meshes require positions and normals");

    // extract vertices
    vertices.reserve(ai_mesh->mNumVertices);
    for (size_t i = 0; i < vertices.capacity(); i++) {
        Vertex v = {};
        v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
        if (ai_mesh->HasTextureCoords(0)) {
            v.uv = { ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
        }
        if (ai_mesh->HasNormals()) {
            v.normal = { ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
        }

        if (ai_mesh->HasTangentsAndBitangents()) {
            v.tangent = { ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z };
            v.binormal = { ai_mesh->mBitangents[i].x, ai_mesh->mBitangents[i].y, ai_mesh->mBitangents[i].z };
        }
        vertices.push_back(std::move(v));
    }
    // extract indices
    indices.reserve(ai_mesh->mNumFaces);
    for (size_t i = 0; i < indices.capacity(); i++) {
        m_assert((ai_mesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
        indices.push_back({ ai_mesh->mFaces[i].mIndices[0], ai_mesh->mFaces[i].mIndices[1], ai_mesh->mFaces[i].mIndices[2] });
    }

    meshes.push_back(Raekor::Mesh(ai_mesh->mName.C_Str(), vertices, indices));
    meshes.back().get_vertex_buffer()->set_layout({ 
        {"POSITION", ShaderType::FLOAT3 },
        {"UV",       ShaderType::FLOAT2 },
        {"NORMAL",   ShaderType::FLOAT3 },
        {"TANGENT",  ShaderType::FLOAT3 },
        {"BINORMAL", ShaderType::FLOAT3 }

    });
}

static auto importer = std::make_shared<Assimp::Importer>();

void Model::load_from_disk() {

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

    scene = importer->ReadFile(path, flags);
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    reload();
}

void Model::reload() {
    if (!scene) return;
    meshes.clear();
    albedos.clear();
    normalMaps.clear();

    for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
            load_mesh(index);
    }

    std::map<std::string, int> seen;
    std::map<std::string, int> seenNormals;
    for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
        auto ai_mesh = scene->mMeshes[m];
        
        
        std::string texture_path, normal_path;
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        aiString albedoFile, normalmapFile;
        material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &albedoFile);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

        texture_path = get_file(path, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
        normal_path = get_file(path, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());

        bool hasNormalMap = !normal_path.empty();

        // fly weight pattern, re-use textures when possible
        if (!texture_path.empty()) {
            if (seen.find(texture_path) == seen.end()) {
                textureIndices.push_back(ti);
                seen[texture_path] = ti;

                if (hasNormalMap) {
                    normalIndices.push_back(ti);
                    seenNormals[normal_path] = ti;
                } else {
                    normalIndices.push_back(-1);
                }
                
                ti++;

            } else {
                textureIndices.push_back(seen[texture_path]);
                if (hasNormalMap) normalIndices.push_back(seenNormals[normal_path]);
                else normalIndices.push_back(-1);
            }
        } else {
            textureIndices.push_back(-1);
            normalIndices.push_back(-1);
        }
    }

    std::vector<Stb::Image> images ( seen.size() );
    std::vector<Stb::Image> normalImages ( seen.size() );
    for (auto& image : images) {
        image.format = RGBA;
        image.isSRGB = true;
    }
    for (auto& image : normalImages) image.format = RGBA;

    std::vector<std::future<void>> futures;
    for (const auto& kv : seen) {
        futures.push_back(std::async(std::launch::async, &Stb::Image::load, &images[kv.second], kv.first, true));
    }

    for (const auto& kv : seenNormals) {
        futures.push_back(std::async(std::launch::async, &Stb::Image::load, &normalImages[kv.second], kv.first, true));
    }

    for (auto& future : futures) {
        future.wait();
    }

    albedos.reserve(images.size());
    for (const auto& image : images) {
        albedos.push_back(std::shared_ptr<Texture>(Texture::construct(image)));
        if (image.channels == 4) {
            albedos.back()->hasAlpha = true;
        }
    }

    normalMaps.reserve(normalImages.size());
    for (const auto& image : normalImages) {
        normalMaps.push_back(std::shared_ptr<Texture>(Texture::construct(image)));
    }

    // if there are no textures we dont care about render order
    if (albedos.empty()) {
        for (int i = 0; i < meshes.size(); i++) {
            ordered.push_back(i);
        }
    }

    // TODO: right now this algo renders all transparent objects last, based off of
    // their texture having an alpha channel, should implement order-independent transparency
    for (int i = 0; i < albedos.size(); i++) {
        for (int j = 0; j < textureIndices.size(); j++) {
            if (textureIndices[j] == i) {
                if (albedos[i]->hasAlpha) {
                    ordered.push_back(j);
                } else {
                    ordered.insert(ordered.begin(), j);
                }
            }
        }
    }
}

void Model::render() const {
    for (int i = 0; i < ordered.size(); i++) {
        int index = ordered[i];
        if (textureIndices[index] != -1) {
            albedos[textureIndices[index]]->bind(0);
        }

        if (normalIndices[index] != -1) {
            normalMaps[normalIndices[index]]->bind(3);
        }

        meshes[index].bind();
        int drawCount = meshes[index].get_index_buffer()->get_count();
        Render::DrawIndexed(drawCount);

    }
    
}

void Model::reset_transform() {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Model::recalc_transform() {
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    auto rotation_quat = static_cast<glm::quat>(rotation);
    transform = transform * glm::toMat4(rotation_quat);
    transform = glm::scale(transform, scale);
}


} // Namespace Raekor