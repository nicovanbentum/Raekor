#pragma once

#include "input.h"

namespace Raekor {

/*
    interface class for native scripts
*/
class NativeScript {
public:
    typedef NativeScript* (__cdecl* FactoryType)();

    virtual ~NativeScript() = default;

    void bind(entt::entity entity, entt::registry& scene);

    virtual void update(float dt) = 0;

    template<typename T>
    T& getComponent() { return scene->get<T>(entity); }

private:
    entt::entity entity;
    entt::registry* scene;
};

namespace ecs {
    struct NativeScriptComponent {
        HMODULE* DLL;
        std::string factoryFN;
        NativeScript* script;
    };
};

void updateNativeScripts(entt::registry& scene, float dt);


class MoveCubeScript : public NativeScript {
public:
    void update(float dt) override;
};

extern "C" __declspec(dllexport) NativeScript * __cdecl CreateMoveScript() {
    return new MoveCubeScript();
}



/*
    update system
*/


} // raekor
