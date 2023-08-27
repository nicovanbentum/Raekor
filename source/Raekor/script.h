#pragma once

#include "ecs.h"
#include "scene.h"

#define RAEKOR_SCRIPT_CLASS(x) extern "C" __declspec(dllexport) Raekor::INativeScript * __cdecl Create##x() { return new class x(); }

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

    void Bind(Raekor::Entity inEntity, Raekor::Scene* inScene);

    virtual void OnUpdate(float dt) = 0;

    template<typename T>
    T& GetComponent();

private:
    Raekor::Scene* m_Scene = nullptr;
    Raekor::Entity m_Entity;
};


template<typename T>
T& INativeScript::GetComponent() {
    return m_Scene->Get<T>(m_Entity);
}

} // Raekor
