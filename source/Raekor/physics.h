#pragma once

#include "pch.h"
#include "util.h"
#include "cvars.h"
#include "async.h"

namespace JPH {
    using ObjectLayer = uint16;
    class BroadPhaseLayer;
    class DebugRenderer;
}

namespace Raekor {

class Scene;
class IRenderer;
class Application;

enum EPhysicsObjectLayers {
    NON_MOVING = 0,
    MOVING = 1,
    NUM_LAYERS = 2,
};

constexpr auto sPhysicsObjectLayersNames = std::array {
    "NON_MOVING",
    "MOVING",
    "NUM_LAYERS"
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
    BPLayerInterfaceImpl() {
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

    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const { return "BroadPhaseLayerName"; }


private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[EPhysicsObjectLayers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const;
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const;
};


class DebugRenderer final : public JPH::DebugRenderer {
public:
    DebugRenderer(IRenderer* inRenderer) : m_Renderer(inRenderer) {}

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor);
    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off);

    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) { return JPH::DebugRenderer::Batch{}; }
    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) { return JPH::DebugRenderer::Batch{}; }

    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid);
    void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) {}

private:
    IRenderer* m_Renderer;
};


class Physics final : public INoCopyNoMove {
public:
    enum EState {
        Idle = 0,
        Paused = 1,
        Stepping = 2
    };

    Physics(IRenderer* inRenderer = nullptr);
    ~Physics();

    void Step(Scene& scene, float dt);
    void OnUpdate(Scene& scene);

    void SaveState()    { m_Physics->SaveState(*m_StateRecorder); }
    void RestoreState() { m_Physics->RestoreState(*m_StateRecorder); };

    bool GetDebugRendering() const         { return m_Debug; }
    void SetDebugRendering(bool inEnabled) { m_Debug = inEnabled; }

    EState GetState() const         { return EState(m_Settings.state); }
    void   SetState(EState inState) { m_Settings.state = inState; }

    void GenerateRigidBodiesEntireScene(Scene& inScene);

    const ImVec4& GetStateColor();

    JPH::PhysicsSystem* GetSystem() { return m_Physics; }


private:
    struct {
        int& state = g_CVars.Create("physics_state", int(Idle), true);
    } m_Settings;

    bool                        m_Debug = true;
    JPH::PhysicsSystem*         m_Physics = nullptr;
    JPH::JobSystem*             m_JobSystem = nullptr;
    JPH::TempAllocator*         m_TempAllocator = nullptr;
    JPH::StateRecorder*         m_StateRecorder = nullptr;
    BPLayerInterfaceImpl        m_BroadPhaseLayers;
    ObjectLayerPairFilter       m_ObjectPairFilter;
    ObjectVsBroadPhaseLayerFilter m_ObjectBroadPhaseFilter;
};

}
