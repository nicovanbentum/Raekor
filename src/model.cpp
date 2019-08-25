#include "pch.h"
#include "model.h"
#include "mesh.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

Model::Model(const std::string& m_file) : path(m_file) {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Model::load_from_disk() {

    constexpr unsigned int flags =
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_PreTransformVertices |
        aiProcess_GenNormals |
        aiProcess_GenUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_Debone |
        aiProcess_ValidateDataStructure;

    Assimp::Importer importer;
    const auto scene = importer.ReadFile(path, flags);
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");


    for (size_t index = 0; index < scene->mNumMeshes; index++) {
        Mesh mesh;
        std::string texture_path;
        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        auto ai_mesh = scene->mMeshes[index];
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString filename;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
            texture_path = get_file(path, PATH_OPTIONS::DIR) + std::string(filename.C_Str());
        }
        m_assert(ai_mesh->HasPositions() && ai_mesh->HasNormals(), "meshes require positions and normals");

        // extract vertices
        vertices.reserve(ai_mesh->mNumVertices);
        for (size_t i = 0; i < vertices.capacity(); i++) {
            Vertex v;
            v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
            if (ai_mesh->HasTextureCoords(0)) {
                v.uv = { ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
            }
            if (ai_mesh->HasNormals()) {
                v.normal = { ai_mesh->mNormals->x, ai_mesh->mNormals->y, ai_mesh->mNormals->z };
            }
            vertices.push_back(std::move(v));
        }
        // extract indices
        indices.reserve(ai_mesh->mNumFaces);
        for (size_t i = 0; i < indices.capacity(); i++) {
            m_assert(ai_mesh->mFaces[i].mNumIndices == 3, "faces require 3 indices");
            indices.push_back({ ai_mesh->mFaces[i].mIndices[0], ai_mesh->mFaces[i].mIndices[1], ai_mesh->mFaces[i].mIndices[2] });
        }

        mesh.set_vertex_buffer(vertices);
        mesh.set_index_buffer(indices);
        meshes.push_back(std::move(mesh));
        textures.push_back(Texture::construct(texture_path));
    }

}

void Model::render() const {
    // TODO: right now we use the index to check if a mesh has a texture
    // maybe these two should be combined in a class or at least an std::pair?
    for (unsigned int i = 0; i < meshes.size(); i++) {
            meshes[i].bind();
            if (textures[i] != nullptr) {
                textures[i]->bind();
            }
            Render::DrawIndexed(meshes[i].get_index_buffer()->get_count());
        }
}

Model::operator bool() const {
    return !meshes.empty();
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