#pragma once

#define RAEKOR_SCRIPT_CLASS(x) extern "C" __declspec(dllexport) Raekor::INativeScript * __cdecl x() { return new class x(); }

namespace Raekor {
/*
    interface class for native scripts
*/
class INativeScript {
public:
    typedef INativeScript* (__cdecl* FactoryType)();

    virtual ~INativeScript() = default;

    void bind(entt::entity entity, entt::registry& scene);

    virtual void update(float dt) = 0;

    template<typename T>
    T& GetComponent() { return scene->get<T>(entity); }

private:
    entt::entity entity;
    entt::registry* scene;
};

class ScriptManager {

};

/* Example

class MoveCubeScript : public NativeScript {
public:
    void update(float dt) override;
};

*/

} // Raekor
