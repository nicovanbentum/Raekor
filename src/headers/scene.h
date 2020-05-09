#pragma once

#include "pch.h"
#include "mesh.h"
#include "camera.h"
#include "texture.h"

#include "ecs.h"
#include "components.h"

namespace Raekor {

class Scene {
public:
    ECS::Entity createObject(const std::string& name);

    ECS::MeshComponent& addMesh();

    void remove(ECS::Entity entity);

public:
    ECS::ComponentManager<ECS::NameComponent> names;
    ECS::ComponentManager<ECS::TransformComponent> transforms;
    ECS::ComponentManager<ECS::MeshComponent> meshes;
    ECS::ComponentManager<ECS::MeshRendererComponent> meshRenderers;
    ECS::ComponentManager<ECS::MaterialComponent> materials;

public:
    std::vector<ECS::Entity> entities;
};

class AssimpImporter {
public:
    void loadFromDisk(Scene& scene, const std::string& file);

private:
    void processAiNode(Scene& scene, const aiScene* aiscene, aiNode* node);

    // TODO: every mesh in the file is created as an Entity that has 1 name, 1 mesh and 1 material component
    // we might want to incorporate meshrenderers and seperate entities for materials
    void loadMesh(Scene& scene, aiMesh* assimpMesh, aiMaterial* assimpMaterial, aiMatrix4x4 localTransform);

    void loadTexturesAsync(const aiScene* scene, const std::string& directory);

private:
    std::unordered_map<std::string, Stb::Image> images;
};

////////////////////////////////////////
// EVERYTHING BELOW SHOULD BE DEPRECATED
///////////////////////////////////////


struct PointLight {
    glm::vec3 position;
    glm::vec3 colour;
};

struct DirectionalLight {
    glm::vec3 position;
    glm::vec3 colour;
};

class Transformable {
public:
    Transformable();
    void reset();
    void recalculate();

    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    glm::vec3 minAABB;
    glm::vec3 maxAABB;

    std::array<glm::vec3, 8> boundingBox;
};

class SceneObject : public Mesh, public Transformable {
public:
    SceneObject() {}
    SceneObject(const std::string& fp, std::vector<Vertex>& vb, std::vector<Face>& ib);
    void render();

    bool hasAlpha = false;
    std::string name;
    uint32_t transformationIndex;
    std::unique_ptr<glTexture2D> albedo;
    std::unique_ptr<glTexture2D> normal;
    std::unique_ptr<Mesh> collisionRenderable;
    std::unique_ptr<btTriangleMesh> triMesh;
    std::unique_ptr<btBvhTriangleMeshShape> shape;
};

class DeprecatedScene {
public:
    DeprecatedScene();

    void add(std::string file);

    void erase(std::string name) {
        auto it = std::find_if(objects.begin(), objects.end(), 
            [&](SceneObject& obj) { return obj.name == name; });
        objects.erase(it);
    }

    std::vector<SceneObject>::iterator at(std::string name) {
            return std::find_if(objects.begin(), objects.end(),
                [&](SceneObject& obj) { return obj.name == name; });
    }

    // for automatic for loop stuff
    std::vector<SceneObject>::iterator begin() { return objects.begin(); }
    std::vector<SceneObject>::iterator end() { return objects.end(); }
    std::vector<SceneObject>::const_iterator begin() const { return objects.begin(); }
    std::vector<SceneObject>::const_iterator end() const { return objects.end(); }

    // this scene has a camera, bunch of objects, 1 directional light, 1 point light
    Camera sunCamera;
    PointLight pointLight;
    DirectionalLight dirLight;
    std::vector<SceneObject> objects;

private:
    std::shared_ptr<Assimp::Importer> importer;
};

} // Namespace Raekor