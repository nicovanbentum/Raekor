#pragma once

#define RAEKOR_SCRIPT_CLASS(x) extern "C" __declspec(dllexport) Raekor::INativeScript * __cdecl x() { return new class x(); }

namespace Raekor {

/* Example

class MoveCubeScript : public NativeScript {
public:
    void update(float dt) override;
};

*/

class INativeScript {
public:
    typedef INativeScript* (__cdecl* FactoryType)();

    virtual ~INativeScript() = default;

    void Bind(entt::entity entity, entt::registry& scene);

    virtual void OnUpdate(float dt) = 0;

    template<typename T>
    T& GetComponent();

private:
    entt::entity entity;
    entt::registry* scene;
};


template<typename T>
T& INativeScript::GetComponent() {
    return scene->get<T>(entity);
}

} // Raekor
