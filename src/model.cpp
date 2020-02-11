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
        {"POSITION", ShaderType::FLOAT3},
        {"UV", ShaderType::FLOAT2},
        {"NORMAL", ShaderType::FLOAT3}
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

    Timer scene_timer;
    scene_timer.start();
    scene = importer->ReadFile(path, flags);
    scene_timer.stop();
    std::cout << "ASSIMP TIME: " << scene_timer.elapsed_ms() << std::endl;
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    reload();
}

void Model::reload() {
    if (!scene) return;
    meshes.clear();
    textures.clear();

    for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
            load_mesh(index); // OpenGL needs more work for async support
    }

    std::map<std::string, int> seen;
    for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
        auto ai_mesh = scene->mMeshes[m];

        std::string texture_path;
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString filename;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
            texture_path = get_file(path, PATH_OPTIONS::DIR) + std::string(filename.C_Str());
        }

        if (!texture_path.empty()) {
            if (seen.find(texture_path) == seen.end()) {
                textureIndices.push_back(ti);
                seen[texture_path] = ti;
                ti++;
            }
            else textureIndices.push_back(seen[texture_path]);
        }
        else textureIndices.push_back(-1);
    }


    std::vector<Stb::Image> images = std::vector<Stb::Image>(seen.size());
    for (auto& image : images) {
        image.format = RGBA;
    }

    std::vector<std::future<void>> futures;
    for (const auto& kv : seen) {
        futures.push_back(std::async(std::launch::async, &Stb::Image::load, &images[kv.second], kv.first, true));
    }

    for (auto& future : futures) {
        future.wait();
    }

    textures.reserve(images.size());
    for (const auto& image : images) {
        textures.push_back(std::shared_ptr<Texture>(Texture::construct(image)));
    }
}

void Model::render() const {
    // TODO: right now we use the index to check if a mesh has a texture
    // maybe these two should be combined in a class or at least an std::pair?
    // TODO: we draw the model backwards because this works for sponza's transparency
    // depth order, we will have to do our own depth sorting in the future.
    for (int i = (int)meshes.size()-1; i >= 0; i--) {
        meshes[i].bind();
        if (textureIndices[i] != -1) {
            textures[textureIndices[i]]->bind();
        }
        Render::DrawIndexed(meshes[i].get_index_buffer()->get_count());
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