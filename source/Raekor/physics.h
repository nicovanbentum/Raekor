#pragma once

#include "pch.h"
#include "cvars.h"

namespace Raekor {

class Scene;

class Physics {
public:
    Physics();
    ~Physics();

    void Step(Scene& scene, float dt);
    void OnUpdate(Scene& scene);

    void SaveState() { m_Physics.SaveState(*m_StateRecorder); }
    void RestoreState() { m_Physics.RestoreState(*m_StateRecorder); };

    JPH::PhysicsSystem& GetSystem() { return m_Physics; }

public:
    enum EState {
        Idle = 0,
        Paused = 1,
        Stepping = 2
    };

    struct {
        int& state = CVars::sCreate("physics_state", int(Idle), true);
    } settings;

    enum Layers {
        NON_MOVING = 0,
        MOVING = 1,
        NUM_LAYERS = 2,
    };

private:
    JPH::PhysicsSystem m_Physics;
    JPH::JobSystem* m_JobSystem;
    JPH::TempAllocator* m_TempAllocator;
    JPH::StateRecorder* m_StateRecorder;
};

}
