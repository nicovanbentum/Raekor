#pragma once

#include "pch.h"
#include "mesh.h"
#include "texture.h"

namespace Raekor {

class Transformable {
public:
    Transformable();
    void reset_transform();
    void recalc_transform();

    btRigidBody* rigidBody;
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

class SceneObject : public Mesh, public Transformable {
public:
    SceneObject() {}
    SceneObject(const std::string& fp, const std::vector<Vertex>& vb, const std::vector<Index>& ib);
    void render();

    std::string name;
    uint32_t transformationIndex;
    std::unique_ptr<GLTexture> albedo;
    std::unique_ptr<GLTexture> normal;
};

class Scene {
public:
    Scene() {
        importer = std::make_shared<Assimp::Importer>();
    }

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

    void renderOpaque() {
        for (auto& object : objects) {
            if (!object.albedo->hasAlpha) {
                object.render();
            }
        }
    }

    void renderTransparent() {
        for (auto& object : objects) {
            if (object.albedo->hasAlpha) {
                object.render();
            }
        }
    }

    void render() {
        renderOpaque();
        renderTransparent();
    }

    std::vector<SceneObject> objects;
    std::vector<glm::mat4> transforms;
private:
    std::shared_ptr<Assimp::Importer> importer;
};

} // Namespace Raekor