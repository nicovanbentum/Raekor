#include "pch.h"
#include "scene.h"
#include "model.h"

namespace Raekor {

void Scene::add(const std::string& name) {
    models[name] = Model();
}

void Scene::remove(const std::string& name) {
    models.erase(name);
}

void Scene::set_mesh(const std::string& name, const std::string& path) {
    models[name].set_mesh(path);
}

void Scene::set_texture(const std::string& name, const std::string& path) {
    models[name].set_texture(path);
}

void Scene::rebuild() {
    for (auto& it : models) {
        std::string mesh_path = it.second.get_mesh()->get_mesh_path();
        it.second.set_mesh(mesh_path);
        if (it.second.has_texture()) {
            std::string texture_path = it.second.get_texture()->get_path();
            it.second.set_texture(texture_path);
        }
    }
}

}