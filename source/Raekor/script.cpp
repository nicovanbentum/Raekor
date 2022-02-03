#include "pch.h"
#include "script.h"
#include "input.h"

// this once contained chaiscript, but in the future I'll mainly support native code or integrate Mono for C#

namespace Raekor {

void INativeScript::bind(entt::entity entity, entt::registry& scene) {
    this->entity = entity;
    this->scene = &scene;
}

/*
void MoveCubeScript::update(float dt) {
    auto& transform = getComponent<TransformComponent>();
    auto keyboardState = SDL_GetKeyboardState(NULL);
    if (keyboardState[SDL_SCANCODE_W]) {
        std::puts("CALLING UPDATE FROM DLL");
        transform.matrix = glm::translate(transform.matrix, glm::vec3(0, 0, 1 * dt));
    }
}

extern "C" __declspec(dllexport) NativeScript * __cdecl MoveCubeScript() {
    return new class MoveCubeScript();
}
*/

} // raekor
