#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::AnimatePlayer(float inDeltaTime)
{
    const Model& model = m_PlayerModel;
    const float speed = glm::length(m_PlayerVelocity);
    const float move = glm::clamp(speed / cPlayerBaseSpeed, 0.0f, 1.5f);

    m_PlayerWalkPhase += inDeltaTime * ( 3.0f + speed * 1.9f );

    const float s = glm::sin(m_PlayerWalkPhase);
    const float c = glm::cos(m_PlayerWalkPhase);
    const float breathe = glm::sin(m_Time * 2.0f) * 0.02f;
    const float cast = glm::clamp(m_CastTimer / 0.25f, 0.0f, 1.0f);
    const bool dashing = m_DashTimer > 0.0f;

    float pitch = dashing ? -0.35f : -0.08f * move;
    float height = glm::abs(s) * 0.06f * move;

    if (m_State == STATE_GAME_OVER)
    {
        const float t = glm::clamp(m_PlayerDeathTimer / 0.5f, 0.0f, 1.0f);
        pitch = EaseOutCubic(t) * 1.45f;
        height = 0.0f;
    }

    const Quat rotation = Quat(Vec3(0.0f, m_PlayerYaw, 0.0f)) * Quat(Vec3(pitch, 0.0f, 0.0f));
    SetTransform(m_PlayerEntity, ToWorld(m_PlayerPosition, height), Vec3(1.0f), rotation);
    SetTransform(model.mRoot, ToWorld(m_PlayerPosition, height), Vec3(1.0f), rotation);

    Pose(model, RIG_BODY, Vec3(0.0f, breathe, 0.0f), Vec3(0.0f, 0.0f, s * 0.05f * move));
    Pose(model, RIG_HEAD, Vec3(0.0f, breathe, 0.0f), Vec3(-c * 0.05f * move, 0.0f, 0.0f));
    Pose(model, RIG_LEG_L, Vec3(0.0f), Vec3(s * 0.6f * move, 0.0f, 0.0f));
    Pose(model, RIG_LEG_R, Vec3(0.0f), Vec3(-s * 0.6f * move, 0.0f, 0.0f));
    Pose(model, RIG_ARM_L, Vec3(0.0f), Vec3(-s * 0.5f * move + ( dashing ? 0.8f : 0.0f ), 0.0f, -0.1f));
    Pose(model, RIG_ARM_R, Vec3(0.0f), Vec3(glm::mix(s * 0.3f * move, -1.3f, cast) + ( dashing ? 0.6f : 0.0f ), 0.0f, 0.05f));
}


void DuskSwarmScript::AnimateEnemy(Enemy& ioEnemy, float inDeltaTime)
{
    const EnemyArchetype& archetype = cEnemyArchetypes[ioEnemy.mType];
    const Model& model = ioEnemy.mModel;

    const float speed = glm::length(ioEnemy.mVelocity);
    const float move = glm::clamp(speed / archetype.mSpeed, 0.0f, 1.5f);

    ioEnemy.mWalkPhase += inDeltaTime * ( 2.0f + speed * 2.4f );

    const float s = glm::sin(ioEnemy.mWalkPhase);
    const float c = glm::cos(ioEnemy.mWalkPhase);
    const float idle = glm::sin(m_Time * 2.0f + ioEnemy.mPhase);

    float height = archetype.mHover;
    float pitch = 0.0f;
    float roll = 0.0f;
    float body_scale = 1.0f;
    Vec3 scale = Vec3(ioEnemy.mElite ? 1.35f : 1.0f);

    Vec3 body = Vec3(0.0f), head = Vec3(0.0f), arm_l = Vec3(0.0f), arm_r = Vec3(0.0f), leg_l = Vec3(0.0f), leg_r = Vec3(0.0f), wing_l = Vec3(0.0f), wing_r = Vec3(0.0f);
    Vec3 body_offset = Vec3(0.0f);

    switch (ioEnemy.mType)
    {
        case ENEMY_GHOUL:
        {
            leg_l.x = s * 0.7f * move;
            leg_r.x = -s * 0.7f * move;
            arm_l.x = -0.15f + c * 0.25f;
            arm_r.x = -0.15f - c * 0.25f;
            body_offset.y = glm::abs(s) * 0.06f * move;
            roll = s * 0.08f * move;
            head.z = glm::sin(ioEnemy.mWalkPhase * 0.5f) * 0.18f;
        } break;

        case ENEMY_BAT:
        {
            height += glm::sin(m_Time * 5.0f + ioEnemy.mPhase) * 0.2f;
            const float flap = glm::sin(m_Time * 22.0f + ioEnemy.mPhase) * 0.75f;
            wing_l.z = -flap;
            wing_r.z = flap;
            pitch = -0.15f;
        } break;

        case ENEMY_SKELETON:
        {
            leg_l.x = s * 0.6f * move;
            leg_r.x = -s * 0.6f * move;
            arm_r.x = -s * 0.4f * move;
            body_offset.y = glm::abs(s) * 0.04f * move;
            head.x = idle * 0.05f;

            if (ioEnemy.mAction == ACTION_AIM)
            {
                const float draw = 1.0f - glm::clamp(ioEnemy.mActionTimer / 0.6f, 0.0f, 1.0f);
                arm_l.x = -0.2f * draw;
                arm_r.x = -0.3f - 0.4f * draw;
                arm_r.y = 0.4f * draw;
            }
        } break;

        case ENEMY_BLOATER:
        {
            roll = s * 0.16f * move;
            leg_l.x = s * 0.45f * move;
            leg_r.x = -s * 0.45f * move;
            arm_l.z = -0.3f + c * 0.3f;
            arm_r.z = 0.3f - c * 0.3f;
            body_scale = 1.0f + 0.05f * glm::sin(m_Time * 3.0f + ioEnemy.mPhase);

            if (ioEnemy.mAction == ACTION_FUSE)
            {
                const float fuse = 1.0f - glm::clamp(ioEnemy.mActionTimer / 0.9f, 0.0f, 1.0f);
                body_scale += fuse * 0.35f + glm::sin(m_Time * 40.0f) * 0.04f;
            }
        } break;

        case ENEMY_WRAITH:
        {
            height += glm::sin(m_Time * 2.4f + ioEnemy.mPhase) * 0.15f;
            pitch = -0.25f * move;
            arm_l.x = -0.4f + glm::sin(m_Time * 3.0f + ioEnemy.mPhase) * 0.25f;
            arm_r.x = -0.4f - glm::sin(m_Time * 3.0f + ioEnemy.mPhase) * 0.25f;

            if (ioEnemy.mAction == ACTION_DASH_WINDUP)
            {
                roll = glm::sin(m_Time * 60.0f) * 0.12f;
                arm_l.x = arm_r.x = -1.6f;
            }
            else if (ioEnemy.mAction == ACTION_DASH)
            {
                pitch = -0.7f;
                arm_l.x = arm_r.x = -1.2f;
            }
        } break;

        case ENEMY_BRUTE:
        {
            leg_l.x = s * 0.45f * move;
            leg_r.x = -s * 0.45f * move;
            arm_l.x = -s * 0.3f * move;
            arm_r.x = s * 0.2f * move - 0.2f;
            roll = s * 0.06f * move;
            body_offset.y = glm::abs(s) * 0.05f * move;

            if (ioEnemy.mAction == ACTION_SLAM)
            {
                const float windup = 1.0f - glm::clamp(ioEnemy.mActionTimer / 0.8f, 0.0f, 1.0f);
                arm_r.x = windup < 0.85f ? -2.7f * EaseOutCubic(windup / 0.85f) : glm::mix(-2.7f, 0.4f, ( windup - 0.85f ) / 0.15f);
                pitch = 0.12f * EaseOutCubic(windup);
            }
        } break;

        case ENEMY_LICH:
        {
            height += glm::sin(m_Time * 1.6f) * 0.2f;
            arm_l.x = -0.3f + idle * 0.15f;
            arm_r.x = -0.2f - idle * 0.1f;
            head.y = glm::sin(m_Time * 0.7f) * 0.2f;

            if (ioEnemy.mAction == ACTION_CAST)
            {
                const float raise = 1.0f - glm::clamp(ioEnemy.mActionTimer / 0.7f, 0.0f, 1.0f);
                arm_l.x = arm_r.x = -1.6f * EaseOutCubic(raise);
                arm_l.z = -0.4f * raise;
                arm_r.z = 0.4f * raise;
            }
        } break;
    }

    if (ioEnemy.mSpawnTimer > 0.0f)
    {
        const float t = 1.0f - glm::clamp(ioEnemy.mSpawnTimer / archetype.mSpawnDuration, 0.0f, 1.0f);

        switch (archetype.mSpawnStyle)
        {
            case SPAWN_RISE:
            {
                height -= ( 1.0f - EaseOutCubic(t) ) * archetype.mTop * 1.1f;
                arm_l.x = arm_r.x = -2.4f * ( 1.0f - t );
            } break;

            case SPAWN_SWOOP:
            {
                height += ( 1.0f - t ) * ( 1.0f - t ) * 7.0f;
            } break;

            case SPAWN_FADE:
            {
                scale *= glm::max(EaseOutBack(t), 0.01f);
            } break;
        }
    }

    scale *= 1.0f + ioEnemy.mHitPunch * ioEnemy.mHitPunch * 0.18f;

    SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, height), scale, Quat(Vec3(0.0f, ioEnemy.mYaw, 0.0f)) * Quat(Vec3(pitch, 0.0f, roll)));

    Pose(model, RIG_BODY, body_offset, body, body_scale);
    Pose(model, RIG_HEAD, body_offset, head);
    Pose(model, RIG_ARM_L, body_offset, arm_l);
    Pose(model, RIG_ARM_R, body_offset, arm_r);
    Pose(model, RIG_LEG_L, Vec3(0.0f), leg_l);
    Pose(model, RIG_LEG_R, Vec3(0.0f), leg_r);
    Pose(model, RIG_WING_L, Vec3(0.0f), wing_l);
    Pose(model, RIG_WING_R, Vec3(0.0f), wing_r);
}


void DuskSwarmScript::AnimateDyingEnemy(Enemy& ioEnemy)
{
    const EnemyArchetype& archetype = cEnemyArchetypes[ioEnemy.mType];
    const Model& model = ioEnemy.mModel;

    const float t = glm::clamp(ioEnemy.mDeathTimer / archetype.mDeathDuration, 0.0f, 1.0f);
    const float base_scale = ioEnemy.mElite ? 1.35f : 1.0f;
    const Quat yaw = Quat(Vec3(0.0f, ioEnemy.mYaw, 0.0f));

    switch (archetype.mDeathStyle)
    {
        case DEATH_FALL:
        {
            const float fall = EaseOutCubic(glm::min(t / 0.35f, 1.0f));
            const float sink = glm::max(t - 0.55f, 0.0f) / 0.45f;
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, -sink * 0.9f), Vec3(base_scale * ( 1.0f - sink * 0.3f )), yaw * Quat(Vec3(fall * 1.5f, 0.0f, ioEnemy.mDeathSpin * 0.3f * fall)));

            Pose(model, RIG_BODY, Vec3(0.0f), Vec3(0.0f));
            Pose(model, RIG_HEAD, Vec3(0.0f), Vec3(fall * 0.5f, 0.0f, ioEnemy.mDeathSpin * 0.4f));
            Pose(model, RIG_ARM_L, Vec3(0.0f), Vec3(-2.6f * fall, 0.0f, -0.4f * fall));
            Pose(model, RIG_ARM_R, Vec3(0.0f), Vec3(-2.4f * fall, 0.0f, 0.5f * fall));
            Pose(model, RIG_LEG_L, Vec3(0.0f), Vec3(-0.4f * fall, 0.0f, 0.0f));
            Pose(model, RIG_LEG_R, Vec3(0.0f), Vec3(0.2f * fall, 0.0f, 0.0f));
        } break;

        case DEATH_SHATTER:
        {
            const float time = ioEnemy.mDeathTimer;
            const float shrink = 1.0f - glm::max(t - 0.7f, 0.0f) / 0.3f;
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, 0.0f), Vec3(base_scale), yaw);

            for (int part = 0; part < RIG_PART_COUNT; part++)
            {
                const RigPart& rig = model.mParts[part];

                if (rig.mNode == Node::Null)
                    continue;

                const Vec3 velocity = ioEnemy.mShatterVelocity[part];
                Vec3 offset = velocity * time - Vec3(0.0f, 9.0f * time * time, 0.0f);
                offset.y = glm::max(offset.y, -rig.mRest.y + 0.08f);

                SetTransform(rig.mNode, rig.mRest + offset, Vec3(shrink), glm::angleAxis(time * 9.0f, ioEnemy.mShatterAxis[part]));
            }
        } break;

        case DEATH_SPIN:
        {
            const float time = ioEnemy.mDeathTimer;
            const float height = glm::max(archetype.mHover - 7.0f * time * time, 0.1f);
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, height), Vec3(base_scale * ( 1.0f - t * 0.6f )), yaw * Quat(Vec3(time * 6.0f, time * 14.0f, 0.0f)));
            Pose(model, RIG_WING_L, Vec3(0.0f), Vec3(0.0f, 0.0f, 0.9f));
            Pose(model, RIG_WING_R, Vec3(0.0f), Vec3(0.0f, 0.0f, -0.9f));
        } break;

        case DEATH_POP:
        {
            const float swell = 1.0f + t * 0.5f;
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, 0.0f), Vec3(base_scale * swell, base_scale * swell * 0.9f, base_scale * swell), yaw);
        } break;

        case DEATH_DISSIPATE:
        {
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, archetype.mHover + t * 1.2f), Vec3(base_scale * ( 1.0f - t ), base_scale * ( 1.0f + t * 0.8f ), base_scale * ( 1.0f - t )), yaw * Quat(Vec3(0.0f, t * 4.0f, 0.0f)));
        } break;

        case DEATH_BOSS:
        {
            const Vec3 shake = Vec3(RandomFloat(-1.0f, 1.0f), 0.0f, RandomFloat(-1.0f, 1.0f)) * 0.12f;
            const float shrink = t > 0.85f ? 1.0f - ( t - 0.85f ) / 0.15f : 1.0f;
            SetTransform(model.mRoot, ToWorld(ioEnemy.mPosition, archetype.mHover + t * 1.5f) + shake, Vec3(shrink), yaw * Quat(Vec3(-0.3f * t, 0.0f, 0.0f)));
            Pose(model, RIG_ARM_L, Vec3(0.0f), Vec3(-2.6f, 0.0f, -0.6f));
            Pose(model, RIG_ARM_R, Vec3(0.0f), Vec3(-2.6f, 0.0f, 0.6f));
            Pose(model, RIG_HEAD, Vec3(0.0f), Vec3(-0.6f, 0.0f, 0.0f));
        } break;
    }
}


void DuskSwarmScript::UpdateVisuals(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    if (m_State == STATE_PLAYING)
        m_PlayerYaw = LerpAngle(m_PlayerYaw, GetYaw(m_FacingDirection), 1.0f - glm::exp(-inDeltaTime * 14.0f));

    AnimatePlayer(inDeltaTime);

    if (m_FirstPerson)
        Park(m_PlayerModel.mRoot);

    const bool player_flash = m_InvulnerableTimer > 0.0f && glm::fract(m_Time * 12.0f) < 0.5f;

    for (const Model* model : { &m_PlayerModel, &m_WandModel })
    {
        if (player_flash)
            SetModelMaterial(*model, m_FlashMaterial);
        else
            RestoreModelMaterials(*model);
    }

    for (Enemy& enemy : m_Enemies)
    {
        if (!enemy.mActive)
            continue;

        if (enemy.mDying)
            AnimateDyingEnemy(enemy);
        else
            AnimateEnemy(enemy, inDeltaTime);

        if (enemy.mAura >= 0)
        {
            const float radius = cEnemyArchetypes[enemy.mType].mRadius * 1.6f * ( 1.0f + 0.08f * glm::sin(m_Time * 6.0f) );
            SetTransform(m_EliteAuras[enemy.mAura].mNode, ToWorld(enemy.mPosition, 0.1f), Vec3(radius * 2.0f, 1.2f, radius * 2.0f), Quat(Vec3(0.0f, m_Time * 2.0f, 0.0f)));

            if (RandomFloat(0.0f, 1.0f) < 0.2f)
                Emit(ToWorld(enemy.mPosition, 0.3f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_GOLD, .mCount = 1, .mSpeedMin = 0.5f, .mSpeedMax = 1.5f, .mLifeMin = 0.4f, .mLifeMax = 0.7f, .mSizeMin = 0.1f, .mSizeMax = 0.16f, .mGravity = -3.0f, .mDrag = 1.0f, .mSpread = 0.8f, .mFlat = true });
        }
    }

    for (const Projectile& projectile : m_Projectiles)
    {
        if (projectile.mActive)
            SetTransform(projectile.mNode, ToWorld(projectile.mPosition, 0.9f), Vec3(1.0f), Quat(Vec3(0.0f, GetYaw(projectile.mVelocity), 0.0f)) * Quat(Vec3(0.0f, 0.0f, m_Time * 12.0f)));
    }

    for (const Gem& gem : m_Gems)
    {
        if (gem.mActive)
        {
            const int tier = GetGemTier(gem.mValue);
            const float size = tier == 2 ? 0.75f : tier == 1 ? 0.6f : 0.42f;
            const float height = 0.45f + 0.08f * glm::sin(m_Time * 4.0f + gem.mPhase);
            SetTransform(gem.mModel.mRoot, ToWorld(gem.mPosition, height), Vec3(size), Quat(Vec3(0.0f, m_Time * 2.0f + gem.mPhase, 0.0f)));
        }
    }

    for (const Pickup& pickup : m_Pickups)
    {
        if (!pickup.mActive)
            continue;

        const float appear = EaseOutBack(glm::min(pickup.mTime / 0.4f, 1.0f));
        const float bob = pickup.mType == PICKUP_CHEST ? 0.0f : 0.25f + 0.12f * glm::sin(m_Time * 3.0f + pickup.mPhase);
        const float spin = pickup.mType == PICKUP_CHEST ? 0.6f : m_Time * 1.8f + pickup.mPhase;

        SetTransform(pickup.mModel.mRoot, ToWorld(pickup.mPosition, bob), Vec3(appear * ( pickup.mType == PICKUP_CHEST ? 1.2f : 1.0f )), Quat(Vec3(0.0f, spin, 0.0f)));

        if (pickup.mType == PICKUP_CHEST)
        {
            const float open = pickup.mOpening ? EaseOutBack(glm::min(pickup.mOpenTimer / 0.4f, 1.0f)) : 0.05f * glm::max(glm::sin(m_Time * 5.0f), 0.0f);
            Pose(pickup.mModel, RIG_HEAD, Vec3(0.0f), Vec3(-1.9f * open, 0.0f, 0.0f));
        }

        const float glow = ( pickup.mType == PICKUP_CHEST ? 1.3f : 0.8f ) * ( 1.0f + 0.1f * glm::sin(m_Time * 4.0f + pickup.mPhase) );
        SetTransform(pickup.mGlow, ToWorld(pickup.mPosition, 0.08f), Vec3(glow * 2.0f, 0.8f, glow * 2.0f), Quat(Vec3(0.0f, -m_Time, 0.0f)));
    }

    const int blade_level = m_UpgradeLevels[UPGRADE_BLADES];
    const int blade_count = blade_level > 0 ? glm::min(1 + blade_level, cMaxBlades) : 0;
    const float blade_radius = ( 2.0f + 0.2f * blade_level ) * glm::sqrt(GetAreaMultiplier());

    for (int blade = 0; blade < cMaxBlades; blade++)
    {
        if (blade >= blade_count || m_State == STATE_GAME_OVER)
        {
            Park(m_Blades[blade]);
            continue;
        }

        const float angle = m_BladeAngle + glm::two_pi<float>() * float(blade) / float(blade_count);
        const Vec2 position = m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * blade_radius;
        SetTransform(m_Blades[blade], ToWorld(position, 0.8f), Vec3(1.2f), Quat(Vec3(0.0f, -angle, 0.0f)) * Quat(Vec3(0.0f, m_Time * 14.0f, 0.0f)));

        if (RandomFloat(0.0f, 1.0f) < 0.25f)
            Emit(ToWorld(position, 0.8f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_EMBER, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.8f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.08f, .mSizeMax = 0.14f, .mDrag = 2.0f });
    }
}

} // RK
