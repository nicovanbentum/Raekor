#include "pch.h"
#include "physics.h"
#include "util.h"
#include "scene.h"
#include "application.h"

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
    for (const auto& [entity, transform, mesh, collider] : scene.view<Transform, Mesh, BoxCollider>().each()) {
        if (collider.bodyID.IsInvalid() && collider.settings.GetRefCount()) {
            auto settings = JPH::BodyCreationSettings(
                &collider.settings,
                JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                collider.motionType,
                EPhysicsObjectLayers::MOVING
            );

            collider.settings.SetEmbedded();

            settings.mAllowDynamicOrKinematic = true;
            collider.bodyID = m_Physics.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
        }
        else {
            const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
            const auto rotation = JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
            m_Physics.GetBodyInterface().SetPositionAndRotationWhenChanged(collider.bodyID, position, rotation, JPH::EActivation::DontActivate);
        }
    }
}


void Physics::GenerateRigidBodiesEntireScene(Scene& inScene) {
    for (const auto& [sb_entity, sb_transform, sb_mesh, sb_collider] : inScene.view<Transform, Mesh, BoxCollider>().each()) {
        auto entity = sb_entity;
        auto& transform = sb_transform;
        auto& mesh = sb_mesh;
        auto& collider = sb_collider;

        g_ThreadPool.QueueJob([&]() {
            JPH::TriangleList triangles;

            for (int i = 0; i < mesh.indices.size(); i += 3) {
                auto v0 = mesh.positions[mesh.indices[i]];
                auto v1 = mesh.positions[mesh.indices[i + 1]];
                auto v2 = mesh.positions[mesh.indices[i + 2]];

                v0 *= transform.scale;
                v1 *= transform.scale;
                v2 *= transform.scale;

                triangles.push_back(JPH::Triangle(JPH::Float3(v0.x, v0.y, v0.z), JPH::Float3(v1.x, v1.y, v1.z), JPH::Float3(v2.x, v2.y, v2.z)));
            }

            collider.motionType = JPH::EMotionType::Static;
            JPH::Ref<JPH::ShapeSettings> settings = new JPH::MeshShapeSettings(triangles);

            auto body_settings = JPH::BodyCreationSettings(
                settings,
                JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                collider.motionType,
                EPhysicsObjectLayers::MOVING
            );

            auto& body_interface = m_Physics.GetBodyInterface();
            collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
       });
    }
}


const ImVec4& Physics::GetStateColor() {
    static const auto colors = std::array {
        ImGui::GetStyleColorVec4(ImGuiCol_Text),
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
        ImVec4(0.35f, 0.78f, 1.0f, 1.0f),
    };

    return colors[GetState()];
}

} // namespace Raekor

