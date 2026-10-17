#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

void CoastRallyScript::CreateProps()
{
    JPH::BodyInterface& bodies = GetBodyInterface();

    StaticArray<JPH::RefConst<JPH::Shape>, PROP_TYPE_COUNT> shapes;

    for (int type = 0; type < PROP_TYPE_COUNT; type++)
    {
        const PropArchetype& archetype = cPropArchetypes[type];

        if (archetype.mShape == PROP_SHAPE_CYLINDER)
            shapes[type] = new JPH::CylinderShape(archetype.mHalfExtents.y, archetype.mHalfExtents.x);
        else
            shapes[type] = new JPH::BoxShape(ToJolt(archetype.mHalfExtents));
    }

    m_Props.clear();

    for (const Placement& placement : m_PropPlacements)
    {
        const auto archetype = std::find_if(cPropArchetypes.begin(), cPropArchetypes.end(), [&](const PropArchetype& inArchetype) { return placement.mAsset == inArchetype.mName; });

        if (archetype == cPropArchetypes.end())
            continue;

        Prop& prop = m_Props.emplace_back();
        prop.mType = int(archetype - cPropArchetypes.begin());
        prop.mNode = CreateModel(archetype->mName).mRoot;
        prop.mSpawnPosition = placement.mPosition;
        prop.mSpawnRotation = placement.mRotation;

        JPH::BodyCreationSettings settings(shapes[prop.mType], ToJolt(placement.mPosition), ToJolt(placement.mRotation), JPH::EMotionType::Dynamic, EPhysicsObjectLayers::MOVING);
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = archetype->mMass;
        settings.mFriction = 0.7f;
        settings.mCollisionGroup = GetCollisionGroup(COLLISION_SUBGROUP_OBSTACLE);
        settings.mRestitution = 0.15f;
        settings.mLinearDamping = 0.1f;
        settings.mAngularDamping = 0.2f;

        prop.mBody = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);

        SetTransform(prop.mNode, prop.mSpawnPosition, prop.mSpawnRotation);
    }
}


void CoastRallyScript::DestroyProps()
{
    if (m_Props.empty())
        return;

    JPH::BodyInterface& bodies = GetBodyInterface();

    for (const Prop& prop : m_Props)
    {
        if (prop.mBody.IsInvalid())
            continue;

        bodies.RemoveBody(prop.mBody);
        bodies.DestroyBody(prop.mBody);
    }

    m_Props.clear();
}


void CoastRallyScript::ResetProps()
{
    JPH::BodyInterface& bodies = GetBodyInterface();

    for (Prop& prop : m_Props)
    {
        bodies.SetPositionAndRotation(prop.mBody, ToJolt(prop.mSpawnPosition), ToJolt(prop.mSpawnRotation), JPH::EActivation::DontActivate);
        bodies.SetLinearAndAngularVelocity(prop.mBody, JPH::Vec3::sZero(), JPH::Vec3::sZero());
        bodies.DeactivateBody(prop.mBody);

        SetTransform(prop.mNode, prop.mSpawnPosition, prop.mSpawnRotation);

        prop.mAwake = false;
        prop.mKnocked = false;
    }

    m_PropsKnocked = 0;
}


void CoastRallyScript::UpdateProps()
{
    JPH::BodyInterface& bodies = GetBodyInterface();
    StaticArray<bool, PROP_TYPE_COUNT> played = {};

    for (Prop& prop : m_Props)
    {
        if (!bodies.IsActive(prop.mBody))
        {
            prop.mAwake = false;
            continue;
        }

        JPH::RVec3 position;
        JPH::Quat rotation;
        bodies.GetPositionAndRotation(prop.mBody, position, rotation);

        SetTransform(prop.mNode, ToVec3(position), ToQuat(rotation));

        if (prop.mAwake)
            continue;

        prop.mAwake = true;

        const float speed = glm::length(ToVec3(bodies.GetLinearVelocity(prop.mBody)));

        if (!played[prop.mType] && speed > 1.0f)
        {
            PlaySfxAt(cPropArchetypes[prop.mType].mSound, ToVec3(position), glm::clamp(speed / 10.0f, 0.3f, 1.0f), std::uniform_real_distribution<float>(0.9f, 1.1f)(m_Random));
            played[prop.mType] = true;
        }

        if (!prop.mKnocked)
        {
            prop.mKnocked = true;
            m_PropsKnocked++;
        }
    }
}

} // RK
