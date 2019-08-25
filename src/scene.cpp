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

void Scene::rebuild() {
    for (auto& it : models) {
        if (it.second) {
            it.second.load_from_disk();
        }
    }
}

}