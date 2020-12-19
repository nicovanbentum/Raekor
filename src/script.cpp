#include "pch.h"
#include "script.h"

// this once contained chaiscript, but in the future I'll mainly support native code or integrate Mono for C#

namespace Raekor {

void updateNativeScripts(entt::registry& scene, float dt) {
    auto view = scene.view<ecs::NativeScriptComponent>();
    for (auto entity : view) {
        auto& component = view.get<ecs::NativeScriptComponent>(entity);

        if (!component.DLL) continue;

        if (!component.script) {
            auto factoryFunction = reinterpret_cast<NativeScript::FactoryType>(
                GetProcAddress(*component.DLL, component.factoryFN.c_str()));
            component.script = factoryFunction();
            component.script->bind(entity, scene);
        } else {
            component.script->update(dt);
        }
    }
}

void NativeScript::bind(entt::entity entity, entt::registry& scene) {
    this->entity = entity;
    this->scene = &scene;
}

void MoveCubeScript::update(float dt) {
    auto& transform = getComponent<ecs::TransformComponent>();
    if (Input::isKeyPressed(SDL_SCANCODE_W)) {
        transform.matrix = glm::translate(transform.matrix, glm::vec3(0, 0, 1 * dt));
    }
}

} // raekor
