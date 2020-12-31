#pragma once

#include "ecs.h"
#include "components.h"
#include "async.h"

namespace Raekor {

class Scene {
public:
    Scene();
    ~Scene();
    Scene(const Scene& rhs) = delete;

    // object management
    entt::entity createObject(const std::string& name = "Empty");
    entt::entity pickObject(Math::Ray& ray);
    
    // per frame systems
    void updateTransforms();
    void loadMaterialTextures(const std::vector<entt::entity>& materials);

    // save to disk
    void saveToFile(const std::string& file);
    void openFromFile(const std::string& file);
    
    // get access to the underlying registry using these
    inline operator entt::registry& () { return registry; }
    inline entt::registry* const operator->() { return &registry; }

private:
    entt::registry registry;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class AssimpImporter {
    // class instead of namespace to hide the individual load methods
public:
    AssimpImporter() = delete;

    static bool loadFile(Scene& scene, const std::string& file);

private:
    static std::vector<entt::entity>   loadMaterials(entt::registry& scene, const aiScene* aiscene, const std::string& directory);
    static entt::entity                loadMesh(entt::registry& scene, aiMesh* assimpMesh);
    static void                        loadBones(entt::registry& scene, const aiScene* aiscene, aiMesh* assimpMesh, entt::entity entity);
};

} // Namespace Raekor