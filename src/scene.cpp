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

}