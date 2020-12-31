#pragma once

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

class MoveCubeScript : public NativeScript {
public:
    void update(float dt) override;
};

} // Raekor
