#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

Mesh::Mesh(const std::string& filepath) :
mesh_path(filepath) {
    load_data();
}

void Mesh::load_data() {
    constexpr unsigned int flags =
        //aiProcess_CalcTangentSpace |
        aiProcess_Triangulate;
        //aiProcess_SortByPType |
        //aiProcess_PreTransformVertices |
        //aiProcess_GenNormals |
        //aiProcess_GenUVCoords |
        //aiProcess_OptimizeMeshes |
        //aiProcess_Debone |
        //aiProcess_ValidateDataStructure;
    
    Assimp::Importer importer;
    const auto scene = importer.ReadFile(mesh_path, flags);
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");
    
    auto mesh = scene->mMeshes[0];
    m_assert(mesh->HasPositions() && mesh->HasNormals(), "meshes require positions and normals");

    // extract vertices
    vertexes.reserve(mesh->mNumVertices);
    for (size_t i = 0; i < vertexes.capacity(); i++) {
        vertexes.push_back(Vertex());
        vertexes.back().pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        if (mesh->HasTextureCoords(0)) {
            vertexes.back().uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        }
    }
    // extract indices
    indexes.reserve(mesh->mNumFaces);
    for (size_t i = 0; i < indexes.capacity(); i++) {
        m_assert(mesh->mFaces[i].mNumIndices == 3, "faces require 3 indices");
        indexes.push_back({ mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] });
    }

    vb.reset(VertexBuffer::construct(vertexes));
    ib.reset(IndexBuffer::construct(indexes));
}

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor