#include "pch.h"
#include "scene.h"
#include "model.h"
#include "timer.h"

namespace Raekor {

void Scene::add(const std::string& name) {
    models[name] = Model();
}

void Scene::remove(const std::string& name) {
    models.erase(name);
}

void Scene::rebuild() {
    for (auto& it : models) {
        if (it.second.complete()) {
            Timer timer;
            timer.start();
            it.second.reload();
            timer.stop();
            std::cout << "Mesh Reload : " << timer.elapsed_ms() << " ms \n";
        }
    }
}

}