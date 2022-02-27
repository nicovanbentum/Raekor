#pragma once

#include "pch.h"
#include "cvars.h"

namespace Raekor {

class Scene;

class Physics {
public:
    Physics();
    ~Physics();

    void InitFromScene(Scene& scene);
    void Step(Scene& scene, float dt);

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

private:
    JPH::PhysicsSystem m_Physics;
    JPH::JobSystem* m_JobSystem;
    JPH::TempAllocator* m_TempAllocator;
    JPH::StateRecorder* m_StateRecorder;
};

}
