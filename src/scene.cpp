#include "pch.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {

Transformable::Transformable() :
    transform(1.0f),
    position(0.0f),
    rotation(0.0f),
    scale(1.0f)
{
    btCollisionShape* boxCollisionShape = new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));
    glm::quat orientation = glm::toQuat(transform);
    btDefaultMotionState* motionstate = new btDefaultMotionState(btTransform(
        btQuaternion(orientation.x, orientation.y, orientation.z, orientation.w),
        btVector3(position.x, position.y, position.z)
    ));

    btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0,                  // mass, in kg. 0 -> Static object, will never move.
        motionstate,
        boxCollisionShape,  // collision shape of body
        btVector3(0, 0, 0)    // local inertia
    );

    rigidBody = new btRigidBody(rigidBodyCI);
}


void Transformable::reset_transform() {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Transformable::recalc_transform() {
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    auto rotation_quat = static_cast<glm::quat>(rotation);
    transform = transform * glm::toMat4(rotation_quat);
    transform = glm::scale(transform, scale);
}

SceneObject::SceneObject(const std::string& fp, const std::vector<Vertex>& vbuffer, const std::vector<Index>& ibuffer)
    : Mesh(fp, vbuffer, ibuffer) {
    vb->set_layout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3}
        });
}

void SceneObject::render() {
    if (albedo != nullptr) albedo->bind(0);
    if (normal != nullptr) normal->bind(3);
    bind();
    int drawCount = ib->get_count();
    Render::DrawIndexed(drawCount);
}

void Scene::add(std::string file) {
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

    auto scene = importer->ReadFile(file, flags);
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    std::vector<Stb::Image> albedos;
    std::vector<Stb::Image> normals;

    for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
        m_assert(scene && scene->HasMeshes(), "failed to load mesh");
        auto ai_mesh = scene->mMeshes[index];

        aiString albedoFile, normalmapFile;
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

        std::string texture_path = get_file(file, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
        std::string normal_path = get_file(file, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());

        if (!texture_path.empty()) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = true;
            albedos.push_back(image);
            albedos.back().load(texture_path, true);
        }

        if (!normal_path.empty()) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = false;
            normals.push_back(image);
            normals.back().load(normal_path, true);
        }
    }

    auto root = scene->mRootNode;
    std::cout << root->mName.C_Str() << std::endl;
    std::cout << root->mNumChildren << std::endl;
    for (int i = 0; i < root->mNumChildren; i++) {
        std::cout << root->mChildren[i]->mName.C_Str() << '\n';
    }

    // loop over every mesh and add it as a scene object
    for (uint64_t index = 0; index < scene->mNumMeshes; index++) {
        m_assert(scene && scene->HasMeshes(), "failed to load mesh");


        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        auto ai_mesh = scene->mMeshes[index];
        m_assert(ai_mesh->HasPositions() && ai_mesh->HasNormals(), "meshes require positions and normals");
        std::string name = ai_mesh->mName.C_Str();

        // extract vertices
        vertices.reserve(ai_mesh->mNumVertices);
        for (size_t i = 0; i < vertices.capacity(); i++) {
            Vertex v = {};
            v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z};
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


        objects.push_back(SceneObject(name, vertices, indices));
        objects.back().name = name;

        aiString albedoFile, normalmapFile;
        auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
        material->GetTextureCount(aiTextureType_DIFFUSE);
        material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

        std::string texture_path = get_file(file, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
        std::string normal_path = get_file(file, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());
        std::cout << texture_path << std::endl;
        std::cout << normal_path << std::endl;

        auto albedoIter = std::find_if(albedos.begin(), albedos.end(), [&](const Stb::Image& img) {
            return img.filepath == texture_path;
        });

        auto normalIter = std::find_if(normals.begin(), normals.end(), [&](const Stb::Image& img) {
            return img.filepath == normal_path;
        });

        if (albedoIter != albedos.end()) {
            auto index = albedoIter - albedos.begin();
            objects.back().albedo.reset(new GLTexture(albedos[index]));
            if (albedoIter->channels == 4) {
                std::cout << albedos[index].filepath << " has alpha channel" << std::endl;
                objects.back().albedo->hasAlpha = true;
            }
            else {
                objects.back().albedo->hasAlpha = false;
            }
        }
        if (normalIter != normals.end()) {
            auto index = normalIter - normals.begin();
            objects.back().normal.reset(new GLTexture(normals[index]));
            objects.back().normal.reset(new GLTexture(normals[index]));
        }
    }
}

} // Namespace Raekor