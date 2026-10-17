#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

Vec2 DuskSwarmScript::GetInputDirection() const
{
    Vec2 input = Vec2(0.0f);

    if (m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::UP))    input.y -= 1.0f;
    if (m_Input->IsKeyDown(Key::S) || m_Input->IsKeyDown(Key::DOWN))  input.y += 1.0f;
    if (m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::LEFT))  input.x -= 1.0f;
    if (m_Input->IsKeyDown(Key::D) || m_Input->IsKeyDown(Key::RIGHT)) input.x += 1.0f;

    if (m_FirstPerson)
        input = GetLookRight() * input.x - GetLookForward() * input.y;

    return glm::length(input) > 0.0f ? glm::normalize(input) : input;
}


void DuskSwarmScript::ToggleFirstPerson()
{
    m_FirstPerson = !m_FirstPerson;

    if (m_FirstPerson)
    {
        m_LookYaw = m_PlayerYaw;
        m_LookPitch = 0.0f;
        m_WandTip = ToWorld(m_PlayerPosition, cEyeHeight);

        if (!m_AimHintShown)
        {
            m_AimHintShown = true;
            AddChat("Hold left click to cast Arcane Bolt at your crosshair.", cChatBlue);
        }
    }

    if (Camera* camera = FindComponent<Camera>(m_CameraEntity))
        camera->SetFov(m_FirstPerson ? cFirstPersonFov : cThirdPersonFov);
}


void DuskSwarmScript::TryDash()
{
    if (m_DashCooldownTimer > 0.0f)
        return;

    const Vec2 input = GetInputDirection();
    m_DashDirection = glm::length(input) > 0.0f ? input : m_FacingDirection;
    m_DashTimer = cDashDuration;
    m_DashCooldownTimer = cDashCooldown;

    PlaySfx(cDashSound, 0.6f, RandomFloat(0.95f, 1.05f));
    Emit(GetPlayerCenter(), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 14, .mSpeedMin = 3.0f, .mSpeedMax = 8.0f, .mLifeMin = 0.2f, .mLifeMax = 0.4f, .mSizeMin = 0.2f, .mSizeMax = 0.35f, .mDrag = 4.0f, .mFlat = true });
}


void DuskSwarmScript::TryJump()
{
    if (IsAirborne())
        return;

    m_JumpVelocity = cJumpSpeed;

    PlaySfx(cDashSound, 0.3f, RandomFloat(1.25f, 1.4f));
    Emit(ToWorld(m_PlayerPosition, 0.1f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_DIRT, .mCount = 6, .mSpeedMin = 1.0f, .mSpeedMax = 2.5f, .mLifeMin = 0.25f, .mLifeMax = 0.4f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mDrag = 4.0f, .mFlat = true });
}


void DuskSwarmScript::UpdateJump(float inDeltaTime)
{
    if (!IsAirborne())
        return;

    m_JumpVelocity -= cJumpGravity * inDeltaTime;
    m_JumpHeight += m_JumpVelocity * inDeltaTime;

    if (m_JumpHeight > 0.0f)
        return;

    m_JumpHeight = 0.0f;
    m_JumpVelocity = 0.0f;
    m_CameraKick = glm::max(m_CameraKick, 0.15f);

    Emit(ToWorld(m_PlayerPosition, 0.1f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_DIRT, .mCount = 8, .mSpeedMin = 1.5f, .mSpeedMax = 3.0f, .mLifeMin = 0.3f, .mLifeMax = 0.5f, .mSizeMin = 0.15f, .mSizeMax = 0.3f, .mDrag = 4.0f, .mFlat = true });
}


void DuskSwarmScript::UpdatePlayer(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const Vec2 input = GetInputDirection();

    if (m_DashTimer > 0.0f)
    {
        m_PlayerVelocity = m_DashDirection * cDashSpeed;

        Emit(GetPlayerCenter(), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_ARCANE, .mCount = 2, .mSpeedMin = 0.2f, .mSpeedMax = 0.8f, .mLifeMin = 0.2f, .mLifeMax = 0.35f, .mSizeMin = 0.35f, .mSizeMax = 0.55f, .mDrag = 2.0f, .mSpread = 0.4f });
    }
    else
    {
        const Vec2 target_velocity = input * GetMoveSpeed();
        m_PlayerVelocity = glm::mix(m_PlayerVelocity, target_velocity, 1.0f - glm::exp(-inDeltaTime * 18.0f));
    }

    m_DashTimer = glm::max(m_DashTimer - inDeltaTime, -1.0f);
    m_DashCooldownTimer = glm::max(0.0f, m_DashCooldownTimer - inDeltaTime);

    UpdateJump(inDeltaTime);

    m_PlayerPosition += m_PlayerVelocity * inDeltaTime;
    m_PlayerPosition = ResolveObstacles(m_PlayerPosition, cPlayerRadius);
    m_PlayerPosition = ClampToArena(m_PlayerPosition, cPlayerRadius);

    if (m_FirstPerson)
        m_FacingDirection = GetLookForward();
    else if (glm::length(input) > 0.0f)
        m_FacingDirection = input;

    const float max_health = GetMaxHealth();
    m_Health = glm::min(max_health, m_Health + 0.6f * m_UpgradeLevels[UPGRADE_REGEN] * inDeltaTime);

    m_InvulnerableTimer = glm::max(0.0f, m_InvulnerableTimer - inDeltaTime);
    m_CastTimer = glm::max(0.0f, m_CastTimer - inDeltaTime);

    m_StreakTimer -= inDeltaTime;

    if (m_StreakTimer <= 0.0f)
        m_Streak = 0;
}


Vec2 DuskSwarmScript::ResolveObstacles(Vec2 inPosition, float inRadius) const
{
    for (const Obstacle& obstacle : m_Obstacles)
    {
        const Vec2 delta = inPosition - obstacle.mPosition;
        const float distance = glm::length(delta);
        const float min_distance = obstacle.mRadius + inRadius;

        if (distance < min_distance)
            inPosition = obstacle.mPosition + ( distance > 0.0001f ? delta / distance : Vec2(1.0f, 0.0f) ) * min_distance;
    }

    return inPosition;
}


void DuskSwarmScript::DamagePlayer(float inDamage)
{
    if (m_InvulnerableTimer > 0.0f || m_DashTimer > -0.12f || m_State != STATE_PLAYING || *m_GodMode)
        return;

    m_Health -= inDamage;
    m_InvulnerableTimer = cPlayerInvulnerability;
    m_HitFlash = 1.0f;
    AddTrauma(0.3f);

    PlaySfx(cPlayerHurtSound, 0.7f, RandomFloat(0.93f, 1.07f));

    Emit(GetPlayerCenter(), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_DANGER, .mCount = 12, .mSpeedMin = 3.0f, .mSpeedMax = 7.0f, .mLifeMin = 0.2f, .mLifeMax = 0.4f, .mSizeMin = 0.2f, .mSizeMax = 0.3f, .mGravity = 8.0f, .mDrag = 2.0f, .mUpBias = 0.4f });

    if (m_Health <= 0.0f)
    {
        m_Health = 0.0f;
        m_State = STATE_GAME_OVER;
        m_EndTime = m_Time;
        m_PlayerDeathTimer = 0.0f;
        m_BestTime = glm::max(m_BestTime, m_RunTime);
        m_HitStop = 0.2f;
        AddTrauma(0.8f);

        StopMusic();
        PlaySfx(cPlayerDeathSound, 0.9f);

        Emit(GetPlayerCenter(), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_ARCANE, .mCount = 24, .mSpeedMin = 1.0f, .mSpeedMax = 4.0f, .mLifeMin = 0.6f, .mLifeMax = 1.2f, .mSizeMin = 0.3f, .mSizeMax = 0.6f, .mGravity = -1.5f, .mDrag = 1.5f, .mUpBias = 0.6f });
        SpawnLight(GetPlayerCenter(), Vec3(0.35f, 0.85f, 1.0f), 2000000.0f, 14.0f, 0.6f);
    }
}


void DuskSwarmScript::AddTrauma(float inAmount, float inCap)
{
    if (m_Trauma < inCap)
        m_Trauma = glm::min(inCap, m_Trauma + inAmount);
}


void DuskSwarmScript::AddTraumaAt(const Vec3& inPosition, float inAmount)
{
    const float distance = glm::distance(Vec2(inPosition.x, inPosition.z), m_PlayerPosition);
    AddTrauma(inAmount * glm::clamp(1.0f - distance / 30.0f, 0.15f, 1.0f), cAmbientTraumaCap);
}


void DuskSwarmScript::UpdateCamera(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    Transform* transform = FindComponent<Transform>(m_CameraEntity);

    if (transform == nullptr)
        return;

    const Vec3 lookahead = ToWorld(m_PlayerVelocity * 0.25f, 0.0f);
    const Vec3 player = ToWorld(m_PlayerPosition, 0.8f + m_JumpHeight * 0.5f) + lookahead;
    m_CameraTarget = glm::mix(m_CameraTarget, player, 1.0f - glm::exp(-inDeltaTime * 6.0f));

    m_Trauma = glm::max(0.0f, m_Trauma - inDeltaTime * 1.4f);
    m_HitFlash = glm::max(0.0f, m_HitFlash - inDeltaTime * 3.0f);
    m_ScreenFlash = glm::max(0.0f, m_ScreenFlash - inDeltaTime * 2.5f);
    m_CameraKick = glm::max(0.0f, m_CameraKick - inDeltaTime * 4.0f);

    const float shake_scale = glm::clamp(*m_ShakeScale, 0.0f, 1.0f);
    const float shake = m_Trauma * m_Trauma * shake_scale;
    const float noise_time = m_Time * 28.0f;
    const Vec3 offset = Vec3(glm::sin(noise_time * 1.1f) + glm::sin(noise_time * 2.3f) * 0.5f, glm::sin(noise_time * 1.7f + 1.3f) + glm::sin(noise_time * 3.1f) * 0.5f, glm::sin(noise_time * 1.3f + 2.1f)) * shake * 0.9f;
    const float roll = glm::sin(noise_time * 0.9f + 0.4f) * shake * 0.06f;

    if (m_FirstPerson)
    {
        const float bob = IsAirborne() ? 0.0f : glm::abs(glm::sin(m_PlayerWalkPhase)) * 0.05f * glm::clamp(glm::length(m_PlayerVelocity) / cPlayerBaseSpeed, 0.0f, 1.0f);
        const float eye_height = m_State == STATE_GAME_OVER ? glm::mix(cEyeHeight, 0.3f, EaseOutCubic(m_PlayerDeathTimer / 0.5f)) : cEyeHeight + bob + m_JumpHeight - m_CameraKick * 0.3f;
        const Vec2 look = GetLookForward();
        const Vec3 forward = Vec3(look.x * glm::cos(m_LookPitch), glm::sin(m_LookPitch), look.y * glm::cos(m_LookPitch));

        transform->position = ToWorld(m_PlayerPosition, eye_height) + offset * 0.15f;
        transform->rotation = glm::angleAxis(roll, forward) * glm::quatLookAtRH(forward, Camera::cUp);
        transform->Compose();
        return;
    }

    const float pitch = glm::radians(58.0f);
    const float distance = m_CameraDistance - m_CameraKick * shake_scale;
    const Vec3 position = m_CameraTarget + Vec3(0.0f, glm::sin(pitch), glm::cos(pitch)) * distance + offset;

    transform->position = position;
    transform->rotation = glm::angleAxis(roll, glm::normalize(m_CameraTarget - position)) * glm::quatLookAtRH(glm::normalize(m_CameraTarget - position), Camera::cUp);
    transform->Compose();
}


void DuskSwarmScript::UpdateWand()
{
    const Transform* camera = FindComponent<Transform>(m_CameraEntity);

    if (!m_FirstPerson || camera == nullptr || m_State == STATE_GAME_OVER)
    {
        Park(m_WandModel.mRoot);
        return;
    }

    const float move = IsAirborne() ? 0.0f : glm::clamp(glm::length(m_PlayerVelocity) / cPlayerBaseSpeed, 0.0f, 1.0f);
    const float cast = glm::clamp(m_CastTimer / 0.25f, 0.0f, 1.0f);
    const float idle = glm::sin(m_Time * 1.6f);
    const Vec3 bob = Vec3(glm::cos(m_PlayerWalkPhase) * 0.012f * move, -glm::abs(glm::sin(m_PlayerWalkPhase)) * 0.014f * move + idle * 0.004f, 0.0f) * cWandScale;
    const Vec3 kick = Vec3(0.0f, 0.01f, 0.05f) * cast * cWandScale;

    const Quat rotation = camera->rotation * Quat(Vec3(cWandPitch + 0.3f * cast + idle * 0.01f, cWandYaw, 0.0f));
    const Vec3 position = camera->position + camera->rotation * ( cWandOffset + bob + kick );

    SetTransform(m_WandModel.mRoot, position, Vec3(cWandScale), rotation);
    m_WandTip = position + rotation * cWandTip;
}

} // RK
