#include "pch.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {

ECS::Entity Scene::createObject(const std::string& name) {
    ECS::Entity entity = ECS::newEntity();
    names.create(entity).name = name;
    transforms.create(entity);
    entities.push_back(entity);
    return entity;
}

ECS::MeshComponent& Scene::addMesh() {
    ECS::Entity entity = createObject("Mesh");
    return meshes.create(entity);
}

void Scene::remove(ECS::Entity entity) {
    names.remove(entity);
    transforms.remove(entity);
    meshes.remove(entity);
    materials.remove(entity);
}

void AssimpImporter::loadFromDisk(Scene& scene, const std::string& file) {
    constexpr unsigned int flags =
        aiProcess_GenNormals |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_PreTransformVertices |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_GenBoundingBoxes |
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

void AssimpImporter::processAiNode(Scene& scene, const aiScene* aiscene, aiNode* node) {
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

void AssimpImporter::loadMesh(Scene& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform) {
    // create new entity and attach a mesh component
    ECS::Entity entity = ECS::newEntity();
    ECS::MeshComponent& mesh = scene.meshes.create(entity);

    ECS::NameComponent& name = scene.names.create(entity);
    name.name = assimpMesh->mName.C_Str();

    ECS::TransformComponent& transform = scene.transforms.create(entity);

    aiVector3D position, scale, rotation;
    localTransform.Decompose(scale, rotation, position);

    transform.position = { position.x, position.y, position.z };
    transform.scale = { scale.x, scale.y, scale.z };
    transform.rotation = { glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z) };

    transform.recalculateMatrix();

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

    mesh.generateAABB();

    transform.localPosition = (mesh.aabb[0] + mesh.aabb[1]) / 2.0f;

    // upload the mesh buffers to the GPU
    mesh.vertexBuffer.bind();
    mesh.vertexBuffer.loadVertices(mesh.vertices.data(), mesh.vertices.size());
    mesh.vertexBuffer.setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3}
        });

    mesh.indexBuffer.bind();
    mesh.indexBuffer.loadFaces(mesh.indices.data(), mesh.indices.size());

    // get material textures from Assimp's import
    aiString albedoFile, normalmapFile;

    assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE);
    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);

    ECS::MaterialComponent& material = scene.materials.create(entity);


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
        material.normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
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

////////////////////////////////////////
// EVERYTHING BELOW SHOULD BE DEPRECATED
///////////////////////////////////////

Transformable::Transformable() :
    transform(1.0f),
    position(0.0f),
    rotation(0.0f),
    scale(1.0f)
{}

void Transformable::reset() {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Transformable::recalculate() {
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    auto rotationQuat = static_cast<glm::quat>(rotation);
    transform = transform * glm::toMat4(rotationQuat);
    transform = glm::scale(transform, scale);
}

SceneObject::SceneObject(const std::string& fp, std::vector<Vertex>& vbuffer, std::vector<Face>& ibuffer)
    : Mesh(vbuffer, ibuffer) {
    vb->setLayout({
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
    int drawCount = ib->count;
    Renderer::DrawIndexed(drawCount);
}

DeprecatedScene::DeprecatedScene() : 
    sunCamera(  glm::vec3(0, 15.0, 0),  glm::orthoRH_ZO(-16.0f, 16.0f, -16.0f, 16.0f, 1.0f, 20.0f))
{
    sunCamera.getView() = glm::lookAtRH(
        glm::vec3(-2.0f, 12.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    sunCamera.getAngle().y = -1.325f;
    importer = std::make_shared<Assimp::Importer>();
}

void DeprecatedScene::add(std::string file) {
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

    if (!scene) {
        std::cout << importer->GetErrorString() << '\n';
    }

    m_assert(scene && scene->HasMeshes(), "failed to load mesh");

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
        std::vector<Face> indices;

        std::cout << "loading " << mesh->mName.C_Str() << "..." << std::endl;

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

        object.collisionRenderable.reset(Mesh::createCube({ object.minAABB, object.maxAABB }));
        object.collisionRenderable->getVertexBuffer()->setLayout({
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

        // hardcoded, every object has at least a diffuse texture,
        // even 1x1 just for color
        objects.back().albedo.reset(new glTexture2D());
        auto& albedo = *objects.back().albedo;

        if (albedoIter != albedos.end()) {
            auto index = albedoIter - albedos.begin();
            auto& image = albedos[index];

            albedo.bind();
            albedo.init(image.w, image.h, Format::SRGBA_U8, image.pixels);
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
            normal.init(image.w, image.h, Format::RGBA_U8, image.pixels);
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