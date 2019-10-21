#include "pch.h"
#include "scene.h"
#include "model.h"
#include "timer.h"

namespace Raekor {

void Scene::add(const std::string& fp) {
    if (models.find(fp) == models.end()) {
        models[fp] = Model(fp);
    }
}

void Scene::remove(const std::string& name) {
    models.erase(name);
}

void Scene::set_key(const std::string& old_key, const std::string& new_key) {
    auto model = models.extract(old_key);
    model.key() = new_key;
    models.insert(std::move(model));
}


void Scene::rebuild() {
    for (auto& it : models) {
            Timer timer;
            timer.start();
            it.second.reload();
            timer.stop();
            std::cout << "Mesh Reload : " << timer.elapsed_ms() << " ms \n";
    }
}

}