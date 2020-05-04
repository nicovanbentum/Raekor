#pragma once

#include "pch.h"
#include "mesh.h"
#include "camera.h"
#include "texture.h"

namespace Raekor {

// This is all really ugly and super hacky, should refactor to a pseudo ECS system

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

class Scene {
public:
    Scene();

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