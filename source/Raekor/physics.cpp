#include "pch.h"
#include "physics.h"
#include "util.h"
#include "scene.h"
#include "async.h"

namespace Raekor {

static void TraceImpl(const char* inFMT, ...) {
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);

    // Print to the TTY
    std::cout << buffer << '\n';
}


// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
    // Print to the TTY
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << '\n';

    // Breakpoint
    return true;
};


// Function that determines if two object layers can collide
static bool CanObjectsCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) {
    switch (inObject1)
    {
    case EPhysicsObjectLayers::NON_MOVING:
        return inObject2 == EPhysicsObjectLayers::MOVING; // Non moving only collides with moving
    case EPhysicsObjectLayers::MOVING:
        return true; // Moving collides with everything
    default:
        assert(false);
        return false;
    }
};


// Function that determines if two broadphase layers can collide
static bool CanBroadphaseCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) {
    switch (inLayer1)
    {
    case EPhysicsObjectLayers::NON_MOVING:
        return inLayer2 == BroadPhaseLayers::MOVING;
    case EPhysicsObjectLayers::MOVING:
        return true;
    default:
        assert(false);
        return false;
    }
}

Physics::Physics() {
    JPH::Trace = TraceImpl;
#ifndef NDEBUG
    JPH::AssertFailed = AssertFailedImpl;
#endif
    JPH::RegisterTypes();

    m_Physics.Init(1024, 0, 1024, 1024, m_BroadPhaseLayers, CanBroadphaseCollide, CanObjectsCollide);

    m_TempAllocator = new JPH::TempAllocatorImpl(100 * 1024 * 1024);
    m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    m_StateRecorder = new JPH::StateRecorderImpl();

    std::cout << "JoltPhysics initialized.\n";
}


Physics::~Physics() {
    delete m_TempAllocator;
    delete m_JobSystem;
}


void Physics::Step(Scene& scene, float dt) {
    m_Physics.Update(dt, 1, 1, m_TempAllocator, m_JobSystem);

    auto& body_interface = m_Physics.GetBodyInterface();

    for (const auto& [entity, transform, mesh, collider] : scene.view<Transform, Mesh, BoxCollider>().each()) {
        if (collider.bodyID.IsInvalid())
            continue;

        JPH::Vec3 position;
        JPH::Quat rotation;
        body_interface.GetPositionAndRotation(collider.bodyID, position, rotation);

        transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
        transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
    }
}


void Physics::OnUpdate(Scene& scene) {
    auto scene_view = scene.view<Transform, Mesh, BoxCollider>();

    for (const auto& entity : scene_view) {
        Async::sQueueJob([&]() {
            auto& body_interface = m_Physics.GetBodyInterface();
            auto& collider = scene_view.get<BoxCollider>(entity);
            const auto& transform = scene_view.get<Transform>(entity);

            if (collider.bodyID.IsInvalid()) {
                auto settings = JPH::BodyCreationSettings(
                    &collider.settings,
                    JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                    JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                    collider.motionType,
                    EPhysicsObjectLayers::MOVING
                );

                collider.settings.SetEmbedded();

                settings.mAllowDynamicOrKinematic = true;
                collider.bodyID = body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
            }
            else {
                const auto position = FromGLM(transform.position);
                const auto rotation = FromGLM(transform.rotation);
                body_interface.SetPositionAndRotationWhenChanged(collider.bodyID, position, rotation, JPH::EActivation::DontActivate);
            }
        });
    }

    Async::sWait();
}

} // namespace Raekor

