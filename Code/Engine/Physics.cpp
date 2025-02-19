#include "pch.h"
#include "physics.h"
#include "DebugRenderer.h"
#include "Application.h"
#include "Components.h"
#include "Profiler.h"
#include "Scene.h"

namespace RK {

static void TraceImpl(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);

	// Print to the TTY
	std::cout << buffer << '\n';
}


// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
{
	// Print to the TTY
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << ( inMessage != nullptr ? inMessage : "" ) << '\n';

	// Breakpoint
	return true;
};


Physics::Physics(IRenderInterface* inRenderer)
{
	JPH::RegisterDefaultAllocator();

	JPH::Trace = TraceImpl;
#ifndef NDEBUG
	JPH::AssertFailed = AssertFailedImpl;
#endif
	
	JPH::Factory::sInstance = new JPH::Factory();

	if (inRenderer)
	{
		JPH::DebugRenderer::sInstance = new PhysicsDebugRenderer();
	}
	JPH::RegisterTypes();

	m_Physics = new JPH::PhysicsSystem();
	m_Physics->Init(65536, 0, 65536, 10240, m_BroadPhaseLayers, m_ObjectBroadPhaseFilter, m_ObjectPairFilter);

	m_TempAllocator = new JPH::TempAllocatorImpl(100 * 1024 * 1024);
	m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
	m_StateRecorder = new JPH::StateRecorderImpl();

	std::cout << "[Physics] JoltPhysics initialized.\n";
}


Physics::~Physics()
{
	delete m_TempAllocator;
	delete m_JobSystem;
	delete m_Physics;
	delete JPH::Factory::sInstance;
	delete JPH::DebugRenderer::sInstance;
}


void Physics::Step(Scene& inScene, float inDeltaTime)
{
    if (m_Settings.state != EState::Stepping)
        return;

	PROFILE_FUNCTION_CPU();

    m_TotalTime += inDeltaTime;
    while (m_TotalTime >= cTimeStep)
    {
	    m_Physics->Update(cTimeStep, 1, m_TempAllocator, m_JobSystem);
        m_TotalTime = m_TotalTime - cTimeStep;
    }

	JPH::BodyInterface& body_interface = m_Physics->GetBodyInterface();

	for (const auto& [entity, transform, mesh, collider] : inScene.Each<Transform, Mesh, RigidBody>())
	{
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


void Physics::OnUpdate(Scene& inScene)
{
	PROFILE_FUNCTION_CPU();

   /* for (const auto& [entity, transform, rigid_body] : inScene.Each<Transform, RigidBody>())
    {
        if (rigid_body.bodyID.IsInvalid() && rigid_body.shape != RigidBody::NONE)
        {
            switch (rigid_body.shape)
            {
                case RigidBody::CUBE:
                {
                    g_ThreadPool.QueueJob([&]() 
                    {
                        rigid_body.CreateCubeCollider(*this, rigid_body.cubeBounds);
                        rigid_body.CreateBody(*this, transform);
                        rigid_body.ActivateBody(*this, transform);
                    });
                } break;

                case RigidBody::SPHERE:
                {
                    g_ThreadPool.QueueJob([&]()
                    {
                        rigid_body.CreateSphereCollider(*this, rigid_body.sphereRadius);
                        rigid_body.CreateBody(*this, transform);
                        rigid_body.ActivateBody(*this, transform);
                    });
                } break;
            }
        }
    }
    */
    for (const auto& [entity, transform, mesh, rigid_body] : inScene.Each<Transform, Mesh, RigidBody>())
    {
        if (rigid_body.bodyID.IsInvalid() && rigid_body.shape == RigidBody::MESH)
        {
            g_ThreadPool.QueueJob([&]()
            {
                rigid_body.CreateMeshCollider(*this, mesh, transform);
                rigid_body.CreateBody(*this, transform);
                rigid_body.ActivateBody(*this, transform);
            });
        }
    }

	if (inScene.Any<SoftBody>() && inScene.Count<SoftBody>())
	{
		for (const auto& [entity, transform, soft_body] : inScene.Each<Transform, SoftBody>())
		{
			if (soft_body.mBodyID.IsInvalid() && soft_body.mSharedSettings.GetRefCount())
			{
				JPH::SoftBodyCreationSettings settings = JPH::SoftBodyCreationSettings(
					&soft_body.mSharedSettings,
					JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
					JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
					EPhysicsObjectLayers::NON_MOVING
				);

				settings.mUpdatePosition = false;

				soft_body.mBodyID = m_Physics->GetBodyInterface().CreateAndAddSoftBody(settings, JPH::EActivation::Activate);
			}
			else
			{
				const JPH::Vec3 position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
				const JPH::Quat rotation = JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
				m_Physics->GetBodyInterface().SetPositionAndRotationWhenChanged(soft_body.mBodyID, position, rotation, JPH::EActivation::DontActivate);

				if (JPH::Body* body = m_Physics->GetBodyLockInterface().TryGetBody(soft_body.mBodyID))
				{
					if (!body->IsActive())
						continue;

					if (JPH::SoftBodyMotionProperties* props = (JPH::SoftBodyMotionProperties*)body->GetMotionProperties())
					{
						if (auto mesh = inScene.GetPtr<Mesh>(entity))
						{
							for (auto i = 0u; i < props->GetVertices().size(); i++)
							{
								const auto& pos = props->GetVertex(i).mPosition;
								mesh->positions[i] = Vec3(pos.GetX(), pos.GetY(), pos.GetZ());
								mesh->positions[i] *= Vec3(1.0f) / transform.scale;
							}
						}
					}
				}
			}
		}
	}

	// Debug draw shapes
	if (m_Debug && JPH::DebugRenderer::sInstance)
	{
		JPH::BodyManager::DrawSettings draw_settings;
		draw_settings.mDrawShape = true;
		//draw_settings.mDrawSoftBodyEdgeConstraints = true;
		m_Physics->DrawBodies(draw_settings, JPH::DebugRenderer::sInstance);
	}

    g_ThreadPool.WaitForJobs();
}


void Physics::GenerateRigidBodiesEntireScene(Scene& inScene)
{
	for (const auto& [entity, transform, mesh, rigid_body] : inScene.Each<Transform, Mesh, RigidBody>())
	{
        if (rigid_body.bodyID.IsInvalid())
        {
		    g_ThreadPool.QueueJob([&]()
		    {
                rigid_body.CreateMeshCollider(*this, mesh, transform);
                rigid_body.CreateBody(*this, transform);
                rigid_body.ActivateBody(*this, transform);
		    });
        }
	}

    g_ThreadPool.WaitForJobs();
}


bool ObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const
{
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

bool ObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const
{
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



void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
	g_DebugRenderer.AddLine(Vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()), Vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()), Vec4(inColor.r, inColor.g, inColor.b, inColor.a));
}


void PhysicsDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
{
	g_DebugRenderer.AddLine(Vec3(inV1.GetX(), inV1.GetY(), inV1.GetZ()), Vec3(inV2.GetX(), inV2.GetY(), inV2.GetZ()), Vec4(inColor.r, inColor.g, inColor.b, inColor.a));
	g_DebugRenderer.AddLine(Vec3(inV1.GetX(), inV1.GetY(), inV1.GetZ()), Vec3(inV3.GetX(), inV3.GetY(), inV3.GetZ()), Vec4(inColor.r, inColor.g, inColor.b, inColor.a));
	g_DebugRenderer.AddLine(Vec3(inV2.GetX(), inV2.GetY(), inV2.GetZ()), Vec3(inV3.GetX(), inV3.GetY(), inV3.GetZ()), Vec4(inColor.r, inColor.g, inColor.b, inColor.a));
}


void PhysicsDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
{
	const auto bb_min = inWorldSpaceBounds.mMin;
	const auto bb_max = inWorldSpaceBounds.mMax;
	g_DebugRenderer.AddLineCube(Vec3(bb_min.GetX(), bb_min.GetY(), bb_min.GetZ()), Vec3(bb_max.GetX(), bb_max.GetY(), bb_max.GetZ()), Mat4x4(1.0f), Vec4(inModelColor.r, inModelColor.g, inModelColor.b, inModelColor.a));
}

} // namespace Raekor

