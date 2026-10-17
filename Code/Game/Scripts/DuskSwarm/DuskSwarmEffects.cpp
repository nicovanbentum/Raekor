#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::Emit(const Vec3& inPosition, const ParticleDesc& inDesc)
{
    Array<Particle>& pool = m_Particles[inDesc.mMesh];

    if (pool.empty())
        return;

    const Entity material = m_FXMaterials[inDesc.mMaterial];

    for (int index = 0; index < inDesc.mCount; index++)
    {
        int& cursor = m_ParticleCursor[inDesc.mMesh];
        Particle& particle = pool[cursor];
        cursor = ( cursor + 1 ) % int(pool.size());

        Vec3 direction = RandomDirection();

        if (inDesc.mFlat)
            direction.y = 0.0f;

        direction.y += inDesc.mUpBias;
        direction = glm::length(direction) > 0.0001f ? glm::normalize(direction) : Vec3(0.0f, 1.0f, 0.0f);

        Vec3 offset = RandomDirection() * RandomFloat(0.0f, inDesc.mSpread);

        if (inDesc.mFlat)
            offset.y = 0.0f;

        particle.mActive = true;
        particle.mStretch = inDesc.mMesh == PARTICLE_SPARK;
        particle.mBounce = inDesc.mMesh == PARTICLE_CHUNK;
        particle.mPosition = inPosition + offset;
        particle.mVelocity = direction * RandomFloat(inDesc.mSpeedMin, inDesc.mSpeedMax) + inDesc.mBaseVelocity;
        particle.mAxis = RandomDirection();
        particle.mAngle = RandomFloat(0.0f, glm::two_pi<float>());
        particle.mSpin = RandomFloat(-8.0f, 8.0f);
        particle.mLife = 0.0f;
        particle.mMaxLife = RandomFloat(inDesc.mLifeMin, inDesc.mLifeMax);
        particle.mSize = RandomFloat(inDesc.mSizeMin, inDesc.mSizeMax);
        particle.mEndSize = particle.mSize * inDesc.mEndSize;
        particle.mGravity = inDesc.mGravity;
        particle.mDrag = inDesc.mDrag;

        if (particle.mMaterial != material)
        {
            particle.mMaterial = material;
            SetMaterial(particle.mNode, material);
        }
    }
}


void DuskSwarmScript::UpdateParticles(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Array<Particle>& pool : m_Particles)
    {
        for (Particle& particle : pool)
        {
            if (!particle.mActive)
                continue;

            particle.mLife += inDeltaTime;

            if (particle.mLife >= particle.mMaxLife)
            {
                particle.mActive = false;
                Park(particle.mNode);
                continue;
            }

            particle.mVelocity.y -= particle.mGravity * inDeltaTime;
            particle.mVelocity *= glm::exp(-particle.mDrag * inDeltaTime);
            particle.mPosition += particle.mVelocity * inDeltaTime;
            particle.mAngle += particle.mSpin * inDeltaTime;

            if (particle.mBounce && particle.mPosition.y < 0.05f)
            {
                particle.mPosition.y = 0.05f;
                particle.mVelocity.y = glm::abs(particle.mVelocity.y) * 0.35f;
                particle.mVelocity.x *= 0.6f;
                particle.mVelocity.z *= 0.6f;
                particle.mSpin *= 0.6f;
            }

            const float t = particle.mLife / particle.mMaxLife;
            const float size = glm::mix(particle.mSize, particle.mEndSize, t * t);

            if (particle.mStretch)
            {
                const float speed = glm::length(particle.mVelocity);
                SetTransform(particle.mNode, particle.mPosition, Vec3(size, size, size * ( 1.0f + speed * 0.12f )), LookRotation(particle.mVelocity));
            }
            else
            {
                SetTransform(particle.mNode, particle.mPosition, Vec3(size), glm::angleAxis(particle.mAngle, particle.mAxis));
            }
        }
    }
}


void DuskSwarmScript::SpawnNovaBurst(const Vec3& inPosition, float inRadius)
{
    NovaBurst* target = &m_NovaBursts[0];

    for (NovaBurst& burst : m_NovaBursts)
    {
        if (!burst.mActive)
        {
            target = &burst;
            break;
        }

        if (burst.mLife > target->mLife)
            target = &burst;
    }

    target->mActive = true;
    target->mPosition = inPosition;
    target->mRadius = inRadius;
    target->mRotation = RandomFloat(0.0f, glm::two_pi<float>());
    target->mLife = 0.0f;
}


void DuskSwarmScript::UpdateNovaBursts(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (NovaBurst& burst : m_NovaBursts)
    {
        if (!burst.mActive)
            continue;

        burst.mLife += inDeltaTime;

        const float t = burst.mLife / cNovaBurstLife;

        if (t >= 1.0f)
        {
            burst.mActive = false;

            for (Node node : burst.mNodes)
                Park(node);

            continue;
        }

        const float grow = EaseOutCubic(t);
        const float outer = glm::mix(0.6f, burst.mRadius * 2.1f, grow);
        const float twist = burst.mRotation + grow * 0.5f;
        const StaticArray<float, 3> sizes = { outer, outer * 0.72f, glm::mix(2.0f, 0.6f, t) };
        const StaticArray<float, 3> alphas = { 0.85f * ( 1.0f - t ), glm::pow(1.0f - t, 1.5f), 1.0f - t };
        const StaticArray<float, 3> rotations = { twist, twist + glm::pi<float>() / 12.0f, 0.0f };

        for (size_t layer = 0; layer < burst.mNodes.size(); layer++)
        {
            SetTransform(burst.mNodes[layer], burst.mPosition + Vec3(0.0f, 0.02f * float(layer), 0.0f), Vec3(sizes[layer], 1.0f, sizes[layer]), Quat(Vec3(0.0f, rotations[layer], 0.0f)));

            if (Material* material = FindComponent<Material>(burst.mMaterials[layer]))
                material->albedo.a = alphas[layer];
        }
    }
}


int DuskSwarmScript::SpawnTelegraph(const Vec3& inPosition, float inRadius, float inDuration, Entity inMaterial)
{
    for (int index = 0; index < int(m_Telegraphs.size()); index++)
    {
        Telegraph& telegraph = m_Telegraphs[index];

        if (telegraph.mActive)
            continue;

        telegraph.mActive = true;
        telegraph.mPosition = Vec3(inPosition.x, 0.07f, inPosition.z);
        telegraph.mRadius = inRadius;
        telegraph.mTime = 0.0f;
        telegraph.mDuration = inDuration;

        SetMaterial(telegraph.mDisc, inMaterial);
        SetMaterial(telegraph.mRing, inMaterial);
        return index;
    }

    return -1;
}


void DuskSwarmScript::ReleaseTelegraph(int inIndex)
{
    if (inIndex >= 0 && inIndex < int(m_Telegraphs.size()))
        ReleaseTelegraph(m_Telegraphs[inIndex]);
}


void DuskSwarmScript::ReleaseTelegraph(Telegraph& ioTelegraph)
{
    ioTelegraph.mActive = false;
    Park(ioTelegraph.mDisc);
    Park(ioTelegraph.mRing);
}


void DuskSwarmScript::UpdateTelegraphs(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Telegraph& telegraph : m_Telegraphs)
    {
        if (!telegraph.mActive)
            continue;

        telegraph.mTime += inDeltaTime;

        if (telegraph.mTime > telegraph.mDuration + 0.1f)
        {
            ReleaseTelegraph(telegraph);
            continue;
        }

        const float t = glm::clamp(telegraph.mTime / telegraph.mDuration, 0.0f, 1.0f);
        const float pulse = 1.0f + 0.04f * glm::sin(m_Time * 20.0f);

        SetTransform(telegraph.mDisc, telegraph.mPosition, Vec3(telegraph.mRadius * 2.0f * t, 1.0f, telegraph.mRadius * 2.0f * t));
        SetTransform(telegraph.mRing, telegraph.mPosition + Vec3(0.0f, 0.02f, 0.0f), Vec3(telegraph.mRadius * 2.0f * pulse, 0.6f, telegraph.mRadius * 2.0f * pulse));
    }
}


void DuskSwarmScript::SpawnBeam(const Vec3& inStart, const Vec3& inEnd, float inWidth, float inLife)
{
    for (Beam& beam : m_Beams)
    {
        if (beam.mActive)
            continue;

        beam.mActive = true;
        beam.mStart = inStart;
        beam.mEnd = inEnd;
        beam.mWidth = inWidth;
        beam.mLife = 0.0f;
        beam.mMaxLife = inLife;
        return;
    }
}


void DuskSwarmScript::UpdateBeams(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Beam& beam : m_Beams)
    {
        if (!beam.mActive)
            continue;

        beam.mLife += inDeltaTime;

        if (beam.mLife >= beam.mMaxLife)
        {
            beam.mActive = false;
            Park(beam.mNode);
            continue;
        }

        const Vec3 delta = beam.mEnd - beam.mStart;
        const float width = beam.mWidth * ( 1.0f - beam.mLife / beam.mMaxLife ) * RandomFloat(0.6f, 1.4f);
        SetTransform(beam.mNode, ( beam.mStart + beam.mEnd ) * 0.5f, Vec3(width, width, glm::length(delta)), LookRotation(delta));
    }
}


void DuskSwarmScript::SpawnLight(const Vec3& inPosition, const Vec3& inColor, float inIntensity, float inRadius, float inLife)
{
    FlashLight* target = &m_Lights[0];
    float oldest = -1.0f;

    for (FlashLight& flash : m_Lights)
    {
        if (!flash.mActive)
        {
            target = &flash;
            break;
        }

        if (flash.mLife / flash.mMaxLife > oldest)
        {
            oldest = flash.mLife / flash.mMaxLife;
            target = &flash;
        }
    }

    target->mActive = true;
    target->mColor = inColor;
    target->mIntensity = inIntensity;
    target->mLife = 0.0f;
    target->mMaxLife = inLife;

    SetTransform(target->mEntity, inPosition, Vec3(1.0f));

    if (Light* light = FindComponent<Light>(target->mEntity))
        light->attributes.x = inRadius;
}


void DuskSwarmScript::UpdateLights(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (FlashLight& flash : m_Lights)
    {
        if (!flash.mActive)
            continue;

        flash.mLife += inDeltaTime;

        const float t = glm::clamp(flash.mLife / flash.mMaxLife, 0.0f, 1.0f);
        const float fade = ( 1.0f - t ) * ( 1.0f - t );

        if (Light* light = FindComponent<Light>(flash.mEntity))
            light->color = Vec4(flash.mColor, flash.mIntensity * fade);

        if (t >= 1.0f)
            flash.mActive = false;
    }
}

} // RK
