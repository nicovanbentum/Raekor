#pragma once

#include "pch.h"
#include "util.h"
#include "cvars.h"
#include "async.h"

namespace Raekor {

enum EPhysicsObjectLayers {
    NON_MOVING = 0,
    MOVING = 1,
    NUM_LAYERS = 2,
};

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[EPhysicsObjectLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[EPhysicsObjectLayers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        assert(inLayer < EPhysicsObjectLayers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[EPhysicsObjectLayers::NUM_LAYERS];
};

class Scene;

class Physics final : public INoCopyNoMove {
public:
    enum EState {
        Idle = 0,
        Paused = 1,
        Stepping = 2
    };
    
    Physics();
    ~Physics();

    void Step(Scene& scene, float dt);
    void OnUpdate(Scene& scene);

    void SaveState()    { m_Physics.SaveState(*m_StateRecorder); }
    void RestoreState() { m_Physics.RestoreState(*m_StateRecorder); };

    EState GetState() const         { return EState(m_Settings.state); }
    void   SetState(EState inState) { m_Settings.state = inState; }

    JPH::PhysicsSystem& GetSystem() { return m_Physics; }

private:
    struct {
        int& state = CVars::sCreate("physics_state", int(Idle), true);
    } m_Settings;

    JPH::PhysicsSystem          m_Physics;
    JPH::JobSystem*             m_JobSystem;
    JPH::TempAllocator*         m_TempAllocator;
    JPH::StateRecorder*         m_StateRecorder;
    BPLayerInterfaceImpl        m_BroadPhaseLayers;
};

}
