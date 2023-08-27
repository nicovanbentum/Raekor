#include "pch.h"
#include "physics.h"
#include "util.h"
#include "scene.h"
#include "application.h"
#include "components.h"

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



Physics::Physics(IRenderer* inRenderer) {
    JPH::RegisterDefaultAllocator();

    JPH::Trace = TraceImpl;
#ifndef NDEBUG
    JPH::AssertFailed = AssertFailedImpl;
#endif
    JPH::Factory::sInstance = new JPH::Factory();

    if (inRenderer)
        JPH::DebugRenderer::sInstance = new DebugRenderer(inRenderer);
    JPH::RegisterTypes();

    m_Physics = new JPH::PhysicsSystem();
    m_Physics->Init(65536, 0, 65536, 10240, m_BroadPhaseLayers, m_ObjectBroadPhaseFilter, m_ObjectPairFilter);

    m_TempAllocator = new JPH::TempAllocatorImpl(100 * 1024 * 1024);
    m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    m_StateRecorder = new JPH::StateRecorderImpl();

    std::cout << "[Physics] JoltPhysics initialized.\n";
}


Physics::~Physics() {
    delete m_TempAllocator;
    delete m_JobSystem;
    delete m_Physics;
    delete JPH::Factory::sInstance;
    delete JPH::DebugRenderer::sInstance;
}


void Physics::Step(Scene& scene, float dt) {
    m_Physics->Update(dt, 1, m_TempAllocator, m_JobSystem);

    auto& body_interface = m_Physics->GetBodyInterface();

    for (const auto& [entity, transform, mesh, collider] : scene.Each<Transform, Mesh, BoxCollider>()) {
        if (collider.bodyID.IsInvalid())
            continue;

        JPH::Vec3 position;
        JPH::Quat rotation;
        body_interface.GetPositionAndRotation(collider.bodyID, position, rotation);

        transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
        transform.rotation = glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());

        transform.Compose();
    }
}


void Physics::OnUpdate(Scene& scene) {
    for (const auto& [entity, transform, mesh, collider] : scene.Each<Transform, Mesh, BoxCollider>()) {
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
            collider.bodyID = m_Physics->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
        }
        else {
            const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
            const auto rotation = JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
            m_Physics->GetBodyInterface().SetPositionAndRotationWhenChanged(collider.bodyID, position, rotation, JPH::EActivation::DontActivate);
        }
    }

    for (const auto& [entity, transform, mesh, soft_body] : scene.Each<Transform, Mesh, SoftBody>()) {
        if (soft_body.mBodyID.IsInvalid() && soft_body.mSharedSettings.GetRefCount()) {
            auto settings = JPH::SoftBodyCreationSettings(
                &soft_body.mSharedSettings, 
                JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                EPhysicsObjectLayers::NON_MOVING
            );

            settings.mUpdatePosition = false;

            soft_body.mBodyID = m_Physics->GetBodyInterface().CreateAndAddSoftBody(settings, JPH::EActivation::Activate);
        }
        else {
            const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
            const auto rotation = JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
            m_Physics->GetBodyInterface().SetPositionAndRotationWhenChanged(soft_body.mBodyID, position, rotation, JPH::EActivation::DontActivate);
            
            if (auto body = m_Physics->GetBodyLockInterface().TryGetBody(soft_body.mBodyID)) {
                if (!body->IsActive())
                    continue;

                if (auto props = (JPH::SoftBodyMotionProperties*)body->GetMotionProperties()) {
                    for (auto i = 0u; i < props->GetVertices().size(); i++) {
                        const auto& pos = props->GetVertex(i).mPosition;
                        mesh.positions[i] = Vec3(pos.GetX(), pos.GetY(), pos.GetZ());
                        mesh.positions[i] *= Vec3(1.0f) / transform.scale;
                    }
                }
            }
        }
    }

    // Debug draw shapes
    if (m_Debug && JPH::DebugRenderer::sInstance) {
        JPH::BodyManager::DrawSettings draw_settings;
        draw_settings.mDrawShape = false;
        draw_settings.mDrawSoftBodyEdgeConstraints = true;
        m_Physics->DrawBodies(draw_settings, JPH::DebugRenderer::sInstance);
    }
}


void Physics::GenerateRigidBodiesEntireScene(Scene& inScene) {
    for (const auto& [sb_entity, sb_transform, sb_mesh, sb_collider] : inScene.Each<Transform, Mesh, BoxCollider>()) {
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

            auto& body_interface = m_Physics->GetBodyInterface();
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


bool ObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
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


/// Returns true if two layers can collide

bool ObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const {
    switch (inLayer1)
    {
    case EPhysicsObjectLayers::NON_MOVING:
        return inLayer2 == EPhysicsObjectLayers::MOVING; // Non moving only collides with moving
    case EPhysicsObjectLayers::MOVING:
        return true; // Moving collides with everything
    default:
        assert(false);
        return false;
    }
}


void DebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) {
    m_Renderer->AddDebugLine(Vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()), Vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()));
}



void DebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) {
    m_Renderer->AddDebugLine(Vec3(inV1.GetX(), inV1.GetY(), inV1.GetZ()), Vec3(inV2.GetX(), inV2.GetY(), inV2.GetZ()));
    m_Renderer->AddDebugLine(Vec3(inV1.GetX(), inV1.GetY(), inV1.GetZ()), Vec3(inV3.GetX(), inV3.GetY(), inV3.GetZ()));
    m_Renderer->AddDebugLine(Vec3(inV2.GetX(), inV2.GetY(), inV2.GetZ()), Vec3(inV3.GetX(), inV3.GetY(), inV3.GetZ()));
}


void DebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode) {
    const auto bb_min = inWorldSpaceBounds.GetCenter() - inWorldSpaceBounds.GetExtent() / 2;
    const auto bb_max = inWorldSpaceBounds.GetCenter() + inWorldSpaceBounds.GetExtent() / 2;

    m_Renderer->AddDebugBox(Vec3(bb_min.GetX(), bb_min.GetY(), bb_min.GetZ()), Vec3(bb_max.GetX(), bb_max.GetY(), bb_max.GetZ()));
}

} // namespace Raekor

