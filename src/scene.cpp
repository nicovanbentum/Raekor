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
{}

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
    : Mesh(vbuffer, ibuffer) {
    vb->set_layout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3}
        });
}

void SceneObject::render() {
    if (albedo != nullptr) albedo->bindToSlot(0);
    if (normal != nullptr) normal->bindToSlot(3);
    bind();
    int drawCount = ib->get_count();
    Renderer::DrawIndexed(drawCount);
}

void Scene::add(std::string file) {
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

    auto scene = importer->ReadFile(file, flags);
    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

    if (!scene) {
        std::cout << importer->GetErrorString() << '\n';
    }

    // pre-load all the textures asynchronously
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
            image.filepath = texture_path;
            albedos.push_back(image);
        }

        if (!normal_path.empty()) {
            Stb::Image image;
            image.format = RGBA;
            image.isSRGB = false;
            image.filepath = normal_path;
            normals.push_back(image);
        }
    }

    // asyncronously load textures from disk
    std::vector<std::future<void>> futures;
    for (auto& img : albedos) {
        //img.load(img.filepath, true);
        futures.push_back(std::async(std::launch::async, &Stb::Image::load, &img, img.filepath, true));
    }

    for (auto& img : normals) {
        //img.load(img.filepath, true);
        futures.push_back(std::async(std::launch::async, &Stb::Image::load, &img, img.filepath, true));
    }

    for (auto& future : futures) {
        future.wait();
    }    
    
    auto processMesh = [&](aiMesh * mesh, aiMatrix4x4 localTransform, const aiScene * scene) {
        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        // extract vertices
        vertices.reserve(mesh->mNumVertices);
        for (size_t i = 0; i < vertices.capacity(); i++) {
            Vertex v = {};
            v.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

            if (mesh->HasTextureCoords(0)) {
                v.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            }
            if (mesh->HasNormals()) {
                v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            }

            if (mesh->HasTangentsAndBitangents()) {
                v.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
                v.binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
            }
            vertices.push_back(std::move(v));
        }
        // extract indices
        indices.reserve(mesh->mNumFaces);
        for (size_t i = 0; i < indices.capacity(); i++) {
            m_assert((mesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
            indices.emplace_back(mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2]);
        }

        std::string name = mesh->mName.C_Str();
        objects.push_back(SceneObject(name, vertices, indices));
        // give it the array index as unique ID for ImGui
        objects.back().name = name + "##" + std::to_string(objects.size()-1);
        auto& object = objects.back();
        object.transform = glm::mat4(1.0f);
        object.position = { 0.0f, 0.0f, 0.0f };

        object.triMesh.reset(new btTriangleMesh());
        for (unsigned int i = 0; i < vertices.size(); i += 3) {
            if (i+2 >= vertices.size()) break;
            object.triMesh->addTriangle(
                { vertices[i].pos.x,    vertices[i].pos.y,      vertices[i].pos.z   },
                { vertices[i+1].pos.x,  vertices[i+1].pos.y,    vertices[i+1].pos.z },
                { vertices[i+2].pos.x,  vertices[i+2].pos.y,    vertices[i+2].pos.z });
        }
        object.shape.reset(new btBvhTriangleMeshShape(object.triMesh.get(), false));

        auto minAABB = object.shape->getLocalAabbMin();
        auto maxAABB = object.shape->getLocalAabbMax();

        object.minAABB = { minAABB.x(), minAABB.y(), minAABB.z() };
        object.maxAABB = { maxAABB.x(), maxAABB.y(), maxAABB.z() };

        object.boundingBox = {
            object.minAABB,
            { object.maxAABB[0], object.minAABB[1], object.minAABB[2] } ,
            { object.maxAABB[0], object.maxAABB[1], object.minAABB[2] } ,
            { object.minAABB[0], object.maxAABB[1], object.minAABB[2] } ,

            { object.minAABB[0], object.minAABB[1], object.maxAABB[2] } ,
            { object.maxAABB[0], object.minAABB[1], object.maxAABB[2] } ,
            object.maxAABB,
            { object.minAABB[0], object.maxAABB[1], object.maxAABB[2] }
        };

        object.collisionRenderable.reset(Mesh::fromBounds({ object.minAABB, object.maxAABB }));
        object.collisionRenderable->get_vertex_buffer()->set_layout({
            {"POSITION",    ShaderType::FLOAT3},
            {"UV",          ShaderType::FLOAT2},
            {"NORMAL",      ShaderType::FLOAT3},
            {"TANGENT",     ShaderType::FLOAT3},
            {"BINORMAL",    ShaderType::FLOAT3}
        });

        aiString albedoFile, normalmapFile;
        auto material = scene->mMaterials[mesh->mMaterialIndex];
        
        material->GetTextureCount(aiTextureType_DIFFUSE);
        material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

        std::string texture_path, normal_path;
        if (strcmp(albedoFile.C_Str(), "") != 0) {
            texture_path = get_file(file, PATH_OPTIONS::DIR) + std::string(albedoFile.C_Str());
        }
        if (strcmp(normalmapFile.C_Str(), "") != 0) {
            normal_path = get_file(file, PATH_OPTIONS::DIR) + std::string(normalmapFile.C_Str());
        }

        auto albedoIter = std::find_if(albedos.begin(), albedos.end(), [&](const Stb::Image& img) {
            return img.filepath == texture_path;
            });

        auto normalIter = std::find_if(normals.begin(), normals.end(), [&](const Stb::Image& img) {
            return img.filepath == normal_path;
            });

        // hardcoded, every object has at least a diffuse texture,
        // even 1x1 just for color
        objects.back().albedo.reset(new glTexture2D());
        auto& albedo = *objects.back().albedo;

        if (albedoIter != albedos.end()) {
            auto index = albedoIter - albedos.begin();
            auto& image = albedos[index];

            albedo.bind();
            albedo.init(image.w, image.h, Format::sRGBA, image.pixels);
            albedo.setFilter(Sampling::Filter::Trilinear);
            albedo.genMipMaps();
            albedo.unbind();

            if (image.channels == 4) {
                object.hasAlpha = true;
            }
            else {
                object.hasAlpha = false;
            }
        } else {
            aiColor4D diffuse;
            if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
                albedo.bind();
                Format::Format format = { GL_SRGB_ALPHA, GL_RGBA, GL_FLOAT };
                albedo.init(1, 1, format, &diffuse[0]);
                albedo.setFilter(Sampling::Filter::None);
                albedo.setWrap(Sampling::Wrap::Repeat);
                albedo.unbind();
            }

        }


        if (normalIter != normals.end()) {
            auto index = normalIter - normals.begin();
            auto& image = normals[index];

            objects.back().normal.reset(new glTexture2D());
            auto& normal = *objects.back().normal;
            
            normal.bind();
            normal.init(image.w, image.h, Format::RGBA, image.pixels);
            normal.setFilter(Sampling::Filter::Trilinear);
            normal.genMipMaps();
            normal.unbind();
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

} // Namespace Raekor