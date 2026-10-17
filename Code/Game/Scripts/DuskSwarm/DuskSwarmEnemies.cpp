#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::UpdateEnemies(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    m_Pushes.assign(m_Enemies.size(), Vec2(0.0f));

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        Enemy& enemy = m_Enemies[index];

        if (!enemy.mActive || enemy.mDying)
            continue;

        const float radius = cEnemyArchetypes[enemy.mType].mRadius;

        ForEachEnemyNear(enemy.mPosition, radius + 1.0f, [&](int inOther, Enemy& inOtherEnemy)
        {
            if (inOther == int(index))
                return;

            const Vec2 delta = enemy.mPosition - inOtherEnemy.mPosition;
            const float distance = glm::length(delta);
            const float other_radius = cEnemyArchetypes[inOtherEnemy.mType].mRadius;
            const float min_distance = radius + other_radius;

            if (distance < min_distance)
            {
                const Vec2 direction = distance > 0.0001f ? delta / distance : Vec2(glm::cos(enemy.mPhase), glm::sin(enemy.mPhase));
                m_Pushes[index] += direction * ( min_distance - distance ) * ( other_radius / min_distance );
            }
        });
    }

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        Enemy& enemy = m_Enemies[index];

        if (!enemy.mActive || enemy.mDying)
            continue;

        const EnemyArchetype& archetype = cEnemyArchetypes[enemy.mType];

        enemy.mHitPunch = glm::max(0.0f, enemy.mHitPunch - inDeltaTime * 6.0f);
        enemy.mBladeCooldown = glm::max(0.0f, enemy.mBladeCooldown - inDeltaTime);

        if (enemy.mFlashTimer > 0.0f)
        {
            enemy.mFlashTimer = glm::max(0.0f, enemy.mFlashTimer - inDeltaTime);

            if (enemy.mFlashTimer <= 0.0f)
                RestoreModelMaterials(enemy.mModel);
        }

        if (enemy.mSpawnTimer > 0.0f)
        {
            enemy.mSpawnTimer -= inDeltaTime;
            enemy.mVelocity = Vec2(0.0f);
            continue;
        }

        Vec2 to_player = m_PlayerPosition - enemy.mPosition;
        const float distance = glm::length(to_player);
        to_player = distance > 0.0001f ? to_player / distance : Vec2(0.0f);
        const Vec2 side = Vec2(-to_player.y, to_player.x);

        const float speed = archetype.mSpeed * ( enemy.mElite ? 0.9f : 1.0f );
        Vec2 desired = to_player * speed;
        Vec2 facing = to_player;

        switch (enemy.mType)
        {
            case ENEMY_BAT:
            {
                desired += side * glm::sin(m_RunTime * 3.0f + enemy.mPhase) * 1.5f;
            } break;

            case ENEMY_SKELETON:
            {
                if (enemy.mAction == ACTION_AIM)
                {
                    desired = Vec2(0.0f);
                    enemy.mActionTimer -= inDeltaTime;

                    if (enemy.mActionTimer <= 0.0f)
                    {
                        const Vec2 lead = m_PlayerPosition + m_PlayerVelocity * glm::min(distance / 13.0f, 0.8f) * 0.6f;
                        const Vec2 aim = glm::normalize(lead - enemy.mPosition + Vec2(0.0001f));
                        FireEnemyProjectile(m_Arrows, ToWorld(enemy.mPosition + aim * 0.5f, 1.1f), Vec3(aim.x, 0.0f, aim.y) * 13.0f, 7.0f * ( enemy.mElite ? 1.5f : 1.0f ), 3.0f);
                        PlaySfxAt(cArrowShotSound, ToWorld(enemy.mPosition, 1.1f), 0.5f, RandomFloat(0.9f, 1.1f), 0.06f);
                        enemy.mAction = ACTION_NONE;
                        enemy.mAttackTimer = RandomFloat(2.2f, 3.2f);
                    }
                }
                else
                {
                    if (distance > 10.0f)
                        desired = to_player * speed;
                    else if (distance < 6.5f)
                        desired = -to_player * speed * 0.8f;
                    else
                        desired = side * speed * 0.5f * ( glm::sin(enemy.mPhase) > 0.0f ? 1.0f : -1.0f );

                    enemy.mAttackTimer -= inDeltaTime;

                    if (enemy.mAttackTimer <= 0.0f && distance < 15.0f)
                    {
                        enemy.mAction = ACTION_AIM;
                        enemy.mActionTimer = 0.6f;
                    }
                }
            } break;

            case ENEMY_BLOATER:
            {
                if (enemy.mAction == ACTION_FUSE)
                {
                    desired = Vec2(0.0f);
                    enemy.mActionTimer -= inDeltaTime;

                    if (glm::fract(enemy.mActionTimer * 6.0f) < 0.5f)
                        SetModelMaterial(enemy.mModel, m_FXMaterials[FX_POISON]);
                    else
                        RestoreModelMaterials(enemy.mModel);

                    if (enemy.mActionTimer <= 0.0f)
                    {
                        BloaterBurst(int(index));
                        continue;
                    }
                }
                else if (distance < 2.2f)
                {
                    enemy.mAction = ACTION_FUSE;
                    enemy.mActionTimer = 0.9f;
                    enemy.mTelegraph = SpawnTelegraph(ToWorld(enemy.mPosition, 0.0f), 2.8f, 0.9f, m_PoisonTelegraphMaterial);
                    PlaySfxAt(cFuseSound, ToWorld(enemy.mPosition, 0.6f), 0.6f, RandomFloat(0.95f, 1.05f), 0.1f);
                }
            } break;

            case ENEMY_WRAITH:
            {
                if (enemy.mAction == ACTION_DASH_WINDUP)
                {
                    desired = Vec2(0.0f);
                    enemy.mActionTimer -= inDeltaTime;

                    if (enemy.mActionTimer <= 0.0f)
                    {
                        enemy.mAction = ACTION_DASH;
                        enemy.mActionTimer = 0.45f;
                        enemy.mDashDirection = to_player;
                    }
                }
                else if (enemy.mAction == ACTION_DASH)
                {
                    desired = enemy.mDashDirection * speed * 3.2f;
                    facing = enemy.mDashDirection;
                    enemy.mActionTimer -= inDeltaTime;

                    Emit(ToWorld(enemy.mPosition, 1.1f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_ARCANE, .mCount = 1, .mSpeedMin = 0.1f, .mSpeedMax = 0.5f, .mLifeMin = 0.25f, .mLifeMax = 0.4f, .mSizeMin = 0.25f, .mSizeMax = 0.4f, .mGravity = -1.0f, .mSpread = 0.3f });

                    if (enemy.mActionTimer <= 0.0f)
                    {
                        enemy.mAction = ACTION_NONE;
                        enemy.mAttackTimer = RandomFloat(3.0f, 4.5f);
                    }
                }
                else
                {
                    enemy.mAttackTimer -= inDeltaTime;

                    if (enemy.mAttackTimer <= 0.0f && distance < 9.0f)
                    {
                        enemy.mAction = ACTION_DASH_WINDUP;
                        enemy.mActionTimer = 0.45f;
                    }
                }
            } break;

            case ENEMY_BRUTE:
            {
                if (enemy.mAction == ACTION_SLAM)
                {
                    desired = Vec2(0.0f);
                    facing = enemy.mDashDirection;
                    enemy.mActionTimer -= inDeltaTime;

                    if (enemy.mActionTimer <= 0.0f)
                    {
                        BruteSlam(enemy);
                        enemy.mAction = ACTION_NONE;
                        enemy.mAttackTimer = RandomFloat(2.5f, 3.5f);
                    }
                }
                else
                {
                    enemy.mAttackTimer -= inDeltaTime;

                    if (enemy.mAttackTimer <= 0.0f && distance < 3.4f)
                    {
                        enemy.mAction = ACTION_SLAM;
                        enemy.mActionTimer = 0.8f;
                        enemy.mDashDirection = to_player;
                        enemy.mTelegraph = SpawnTelegraph(ToWorld(enemy.mPosition + to_player * 1.4f, 0.0f), 3.0f * ( enemy.mElite ? 1.3f : 1.0f ), 0.8f, m_DangerMaterial);
                    }
                }
            } break;

            case ENEMY_LICH:
            {
                UpdateLich(enemy, distance, to_player, side, desired, inDeltaTime);
            } break;
        }

        enemy.mPosition += ( desired + enemy.mKnockback ) * inDeltaTime + m_Pushes[index] * 0.5f;
        enemy.mVelocity = desired;
        enemy.mKnockback *= glm::exp(-inDeltaTime * 8.0f);

        if (!archetype.mFlying)
            enemy.mPosition = ResolveObstacles(enemy.mPosition, archetype.mRadius);

        enemy.mPosition = ClampToArena(enemy.mPosition, archetype.mRadius);

        if (glm::length(facing) > 0.0001f)
            enemy.mYaw = LerpAngle(enemy.mYaw, GetYaw(facing), 1.0f - glm::exp(-inDeltaTime * 10.0f));

        const float contact_radius = archetype.mRadius * ( enemy.mElite ? 1.25f : 1.0f ) + cPlayerRadius;

        if (glm::distance(enemy.mPosition, m_PlayerPosition) < contact_radius)
            DamagePlayer(archetype.mDamage * ( enemy.mElite ? 1.5f : 1.0f ));

        if (distance > 60.0f && enemy.mType != ENEMY_LICH)
            Despawn(int(index));
    }
}


void DuskSwarmScript::UpdateLich(Enemy& ioEnemy, float inDistance, const Vec2& inToPlayer, const Vec2& inSide, Vec2& outDesired, float inDeltaTime)
{
    const float speed = cEnemyArchetypes[ENEMY_LICH].mSpeed;
    const bool enraged = ioEnemy.mHealth < ioEnemy.mMaxHealth * 0.5f;

    if (inDistance > 9.0f)
        outDesired = inToPlayer * speed;
    else if (inDistance < 6.0f)
        outDesired = -inToPlayer * speed * 0.6f;
    else
        outDesired = inSide * speed * 0.5f;

    if (ioEnemy.mAction == ACTION_CAST)
    {
        outDesired *= 0.2f;
        ioEnemy.mActionTimer -= inDeltaTime;

        const Vec3 hand = ToWorld(ioEnemy.mPosition, 1.6f);
        Emit(hand + RandomDirection() * 1.5f, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_PURPLE, .mCount = 2, .mSpeedMin = 0.5f, .mSpeedMax = 1.0f, .mLifeMin = 0.2f, .mLifeMax = 0.35f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mDrag = 1.0f });

        if (ioEnemy.mActionTimer > 0.0f)
            return;

        ioEnemy.mAction = ACTION_NONE;
        ioEnemy.mAttackTimer = enraged ? 1.8f : 2.6f;

        const Vec3 origin = ToWorld(ioEnemy.mPosition, 1.4f);

        switch (m_BossPattern)
        {
            case 0:
            {
                const int count = enraged ? 26 : 18;
                const float offset = RandomFloat(0.0f, glm::two_pi<float>());

                for (int orb = 0; orb < count; orb++)
                {
                    const float angle = offset + glm::two_pi<float>() * float(orb) / float(count);
                    const Vec3 direction = Vec3(glm::cos(angle), 0.0f, glm::sin(angle));
                    FireEnemyProjectile(m_Orbs, origin + direction * 1.2f, direction * 6.0f, 12.0f, 5.0f);
                }
            } break;

            case 1:
            {
                const int count = enraged ? 7 : 5;
                const float base = glm::atan(inToPlayer.y, inToPlayer.x);

                for (int orb = 0; orb < count; orb++)
                {
                    const float angle = base + ( float(orb) - float(count - 1) * 0.5f ) * 0.14f;
                    const Vec3 direction = Vec3(glm::cos(angle), 0.0f, glm::sin(angle));
                    FireEnemyProjectile(m_Orbs, origin + direction * 1.2f, direction * 9.5f, 12.0f, 4.0f);
                }
            } break;

            case 2:
            {
                const int count = enraged ? 8 : 5;

                for (int minion = 0; minion < count; minion++)
                {
                    const Vec2 position = ClampToArena(m_PlayerPosition + RandomCircle() * RandomFloat(6.0f, 8.0f), 1.0f);
                    SpawnEnemy(minion % 3 == 2 ? ENEMY_GHOUL : ENEMY_SKELETON, position);
                }

                AddChat("The dead answer the Lich's call.", cChatPurple);
            } break;
        }

        m_BossPattern = ( m_BossPattern + 1 ) % 3;
        PlaySfxAt(cNovaSound, origin, 0.6f, 0.7f);
        SpawnLight(origin, Vec3(0.75f, 0.35f, 1.0f), 1500000.0f, 10.0f, 0.4f);
        AddTraumaAt(origin, 0.12f);
        return;
    }

    ioEnemy.mAttackTimer -= inDeltaTime;

    if (ioEnemy.mAttackTimer <= 0.0f)
    {
        ioEnemy.mAction = ACTION_CAST;
        ioEnemy.mActionTimer = 0.7f;
        PlaySfxAt(cLichCastSound, ToWorld(ioEnemy.mPosition, 1.6f), 0.8f, enraged ? 0.9f : 1.0f);
    }

    if (RandomFloat(0.0f, 1.0f) < 0.4f)
        Emit(ToWorld(ioEnemy.mPosition, 0.4f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_PURPLE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.6f, .mLifeMin = 0.5f, .mLifeMax = 0.9f, .mSizeMin = 0.2f, .mSizeMax = 0.4f, .mGravity = -1.0f, .mSpread = 0.8f });
}


void DuskSwarmScript::BruteSlam(Enemy& ioEnemy)
{
    const float radius = 3.0f * ( ioEnemy.mElite ? 1.3f : 1.0f );
    const Vec2 center = ioEnemy.mPosition + ioEnemy.mDashDirection * 1.4f;

    ReleaseTelegraph(ioEnemy.mTelegraph);
    ioEnemy.mTelegraph = -1;

    if (glm::distance(center, m_PlayerPosition) < radius + cPlayerRadius)
        DamagePlayer(22.0f * ( ioEnemy.mElite ? 1.5f : 1.0f ));

    Explode(ToWorld(center, 0.1f), radius, 0.0f, EXPLOSION_DUST, false, 0.0f);
}


void DuskSwarmScript::BloaterBurst(int inIndex)
{
    Enemy& enemy = m_Enemies[inIndex];

    ReleaseTelegraph(enemy.mTelegraph);
    enemy.mTelegraph = -1;

    Explode(ToWorld(enemy.mPosition, 0.6f), 2.8f, 25.0f, EXPLOSION_POISON, true, 18.0f * ( enemy.mElite ? 1.5f : 1.0f ));
    ReleaseAura(enemy);
    Despawn(inIndex);
}


void DuskSwarmScript::Despawn(int inIndex)
{
    Enemy& enemy = m_Enemies[inIndex];

    ReleaseTelegraph(enemy.mTelegraph);
    enemy.mTelegraph = -1;
    ReleaseAura(enemy);

    enemy.mActive = false;
    enemy.mDying = false;
    RestoreModelMaterials(enemy.mModel);
    Park(enemy.mModel.mRoot);
    m_FreeEnemies[enemy.mType].push_back(inIndex);

    if (inIndex == m_BossEnemy)
    {
        m_BossEnemy = -1;

        if (m_State == STATE_PLAYING)
            PlayMusic(false);
    }
}


void DuskSwarmScript::ReleaseAura(Enemy& ioEnemy)
{
    if (ioEnemy.mAura >= 0)
    {
        m_EliteAuras[ioEnemy.mAura].mActive = false;
        Park(m_EliteAuras[ioEnemy.mAura].mNode);
        ioEnemy.mAura = -1;
    }
}


bool DuskSwarmScript::DamageEnemy(int inIndex, float inDamage, const Vec2& inKnockback, bool inCanCrit)
{
    Enemy& enemy = m_Enemies[inIndex];

    if (!enemy.mActive || enemy.mDying)
        return false;

    const bool crit = inCanCrit && RandomFloat(0.0f, 1.0f) < GetCritChance();
    const float damage = crit ? inDamage * 2.0f : inDamage;

    enemy.mHealth -= damage;
    enemy.mKnockback += inKnockback * cEnemyArchetypes[enemy.mType].mKnockback * ( enemy.mElite ? 0.4f : 1.0f );
    enemy.mFlashTimer = cFlashDuration;
    enemy.mHitPunch = 1.0f;
    m_DamageDealt += damage;

    if (enemy.mAction != ACTION_FUSE)
        SetModelMaterial(enemy.mModel, enemy.mElite ? m_EliteFlashMaterial : m_FlashMaterial);

    AddDamageNumber(ToWorld(enemy.mPosition, cEnemyArchetypes[enemy.mType].mTop * ( enemy.mElite ? 1.35f : 1.0f ) + 0.3f), int(glm::ceil(damage)), crit);
    PlaySfxAt(cEnemyHitSound, ToWorld(enemy.mPosition, 0.8f), crit ? 0.45f : 0.3f, RandomFloat(0.85f, 1.15f) * ( crit ? 0.85f : 1.0f ), 0.05f);

    if (enemy.mHealth <= 0.0f)
    {
        KillEnemy(inIndex);
        return true;
    }

    return false;
}


void DuskSwarmScript::KillEnemy(int inIndex)
{
    Enemy& enemy = m_Enemies[inIndex];
    const EnemyArchetype& archetype = cEnemyArchetypes[enemy.mType];

    enemy.mDying = true;
    enemy.mDeathTimer = 0.0f;
    enemy.mDeathSpin = RandomFloat(-1.0f, 1.0f);
    enemy.mDeathImpact = false;

    ReleaseTelegraph(enemy.mTelegraph);
    enemy.mTelegraph = -1;

    m_Kills++;
    m_Streak++;
    m_BestStreak = glm::max(m_BestStreak, m_Streak);
    m_StreakTimer = cStreakWindow;

    if (m_Streak > 0 && m_Streak % 50 == 0)
        AddChat(std::format("Kill streak: {}!", m_Streak), cChatGreen);

    const Vec3 center = ToWorld(enemy.mPosition, archetype.mTop * 0.5f);

    if (enemy.mType == ENEMY_LICH)
        PlaySfx(cLichDeathSound, 1.0f);
    else if (const char* death_sound = cDeathSounds[enemy.mType])
        PlaySfxAt(death_sound, center, enemy.mElite ? 0.8f : 0.5f, RandomFloat(0.9f, 1.1f) * ( enemy.mElite ? 0.8f : 1.0f ), 0.09f);

    switch (archetype.mDeathStyle)
    {
        case DEATH_FALL:
        {
            Emit(ToWorld(enemy.mPosition, 0.2f), ParticleDesc { .mMesh = PARTICLE_CHUNK, .mMaterial = enemy.mType == ENEMY_BRUTE ? FX_FLESH : FX_GOO, .mCount = enemy.mType == ENEMY_BRUTE ? 10 : 5, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.5f, .mLifeMax = 0.9f, .mSizeMin = 0.08f, .mSizeMax = 0.16f, .mEndSize = 0.4f, .mGravity = 16.0f, .mDrag = 0.5f, .mUpBias = 1.2f, .mSpread = 0.3f });
            Emit(center, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_PURPLE, .mCount = 2, .mSpeedMin = 0.3f, .mSpeedMax = 0.8f, .mLifeMin = 0.8f, .mLifeMax = 1.2f, .mSizeMin = 0.15f, .mSizeMax = 0.3f, .mGravity = -2.5f, .mDrag = 1.0f });
        } break;

        case DEATH_SHATTER:
        {
            for (int part = 0; part < RIG_PART_COUNT; part++)
            {
                const Vec2 out = RandomCircle();
                enemy.mShatterVelocity[part] = Vec3(out.x * RandomFloat(1.5f, 4.0f), RandomFloat(3.0f, 6.0f), out.y * RandomFloat(1.5f, 4.0f));
                enemy.mShatterAxis[part] = RandomDirection();
            }

            Emit(center, ParticleDesc { .mMesh = PARTICLE_CHUNK, .mMaterial = FX_BONE, .mCount = 8, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.6f, .mLifeMax = 1.0f, .mSizeMin = 0.06f, .mSizeMax = 0.12f, .mEndSize = 0.5f, .mGravity = 16.0f, .mDrag = 0.4f, .mUpBias = 0.8f });
            Emit(ToWorld(enemy.mPosition, 1.3f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_DANGER, .mCount = 6, .mSpeedMin = 1.0f, .mSpeedMax = 3.0f, .mLifeMin = 0.3f, .mLifeMax = 0.5f, .mSizeMin = 0.12f, .mSizeMax = 0.2f, .mDrag = 2.0f });
        } break;

        case DEATH_SPIN:
        {
            Emit(center, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_PURPLE, .mCount = 6, .mSpeedMin = 1.5f, .mSpeedMax = 4.0f, .mLifeMin = 0.3f, .mLifeMax = 0.5f, .mSizeMin = 0.12f, .mSizeMax = 0.2f, .mGravity = 4.0f, .mDrag = 2.0f });
        } break;

        case DEATH_DISSIPATE:
        {
            Emit(center, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_ARCANE, .mCount = 6, .mSpeedMin = 0.5f, .mSpeedMax = 1.5f, .mLifeMin = 0.6f, .mLifeMax = 1.0f, .mSizeMin = 0.2f, .mSizeMax = 0.4f, .mGravity = -2.0f, .mDrag = 1.5f, .mSpread = 0.4f });
        } break;

        case DEATH_BOSS:
        {
            m_HitStop = 0.15f;
            AddTrauma(0.6f);
            m_LichKills++;
            AddPopup("The Lich is Slain", "Its treasure is yours", Vec4(1.0f, 0.82f, 0.35f, 1.0f));
            AddChat("Congratulations, you have defeated the Lich!", cChatBlue);
            AddChat(std::format("Your Lich kill count is: {}.", m_LichKills), cChatBlack);
            m_BossKills++;
        } break;

        default:
            break;
    }

    if (enemy.mElite)
    {
        m_EliteKills++;
        SpawnPickup(PICKUP_CHEST, enemy.mPosition);
        SpawnLight(center, Vec3(1.0f, 0.8f, 0.3f), 1500000.0f, 10.0f, 0.5f);
        AddTraumaAt(center, 0.2f);
    }

    if (enemy.mType == ENEMY_LICH)
    {
        SpawnPickup(PICKUP_CHEST, enemy.mPosition);

        for (int gem = 0; gem < 6; gem++)
            SpawnGem(enemy.mPosition + RandomCircle() * RandomFloat(1.0f, 2.5f), archetype.mGemValue);
    }
    else
    {
        SpawnGem(enemy.mPosition, archetype.mGemValue * ( enemy.mElite ? 5 : 1 ));

        const float roll = RandomFloat(0.0f, 1.0f);

        if (roll < 0.01f)
            SpawnPickup(PICKUP_POTION, enemy.mPosition);
        else if (roll < 0.015f)
            SpawnPickup(PICKUP_MAGNET, enemy.mPosition);
        else if (roll < 0.019f)
            SpawnPickup(PICKUP_BOMB, enemy.mPosition);
    }
}


void DuskSwarmScript::UpdateDying(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        Enemy& enemy = m_Enemies[index];

        if (!enemy.mActive || !enemy.mDying)
            continue;

        const EnemyArchetype& archetype = cEnemyArchetypes[enemy.mType];
        enemy.mDeathTimer += inDeltaTime;

        const float t = enemy.mDeathTimer / archetype.mDeathDuration;

        if (archetype.mDeathStyle == DEATH_FALL && !enemy.mDeathImpact && t > 0.35f)
        {
            enemy.mDeathImpact = true;
            Emit(ToWorld(enemy.mPosition, 0.1f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_DIRT, .mCount = enemy.mType == ENEMY_BRUTE ? 8 : 3, .mSpeedMin = 1.0f, .mSpeedMax = 2.5f, .mLifeMin = 0.4f, .mLifeMax = 0.8f, .mSizeMin = 0.2f, .mSizeMax = 0.4f, .mGravity = -0.5f, .mDrag = 3.0f, .mSpread = 0.6f, .mFlat = true });

            if (enemy.mType == ENEMY_BRUTE)
                AddTraumaAt(ToWorld(enemy.mPosition, 0.0f), 0.2f);
        }

        if (archetype.mDeathStyle == DEATH_DISSIPATE && RandomFloat(0.0f, 1.0f) < 0.5f)
            Emit(ToWorld(enemy.mPosition, 1.2f), ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_ARCANE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.6f, .mLifeMin = 0.4f, .mLifeMax = 0.7f, .mSizeMin = 0.15f, .mSizeMax = 0.3f, .mGravity = -2.0f, .mSpread = 0.4f });

        if (archetype.mDeathStyle == DEATH_BOSS && glm::fract(enemy.mDeathTimer * 5.5f) < glm::fract(( enemy.mDeathTimer - inDeltaTime ) * 5.5f))
        {
            const Vec3 offset = Vec3(RandomFloat(-1.2f, 1.2f), RandomFloat(0.5f, 3.0f), RandomFloat(-1.2f, 1.2f));
            Explode(ToWorld(enemy.mPosition, 0.0f) + offset, 1.6f, 0.0f, EXPLOSION_ARCANE, false, 0.0f);
        }

        if (enemy.mDeathTimer >= archetype.mDeathDuration)
        {
            if (archetype.mDeathStyle == DEATH_POP)
                Explode(ToWorld(enemy.mPosition, 0.6f), 2.8f, 25.0f, EXPLOSION_POISON, false, 0.0f);

            if (archetype.mDeathStyle == DEATH_BOSS)
            {
                Explode(ToWorld(enemy.mPosition, 1.5f), 6.0f, 0.0f, EXPLOSION_HOLY, false, 0.0f);
                m_ScreenFlash = 0.8f;
                m_HitStop = 0.12f;
            }

            Despawn(int(index));
        }
    }
}


void DuskSwarmScript::FireEnemyProjectile(Array<EnemyProjectile>& ioPool, const Vec3& inPosition, const Vec3& inVelocity, float inDamage, float inLife)
{
    for (EnemyProjectile& projectile : ioPool)
    {
        if (projectile.mActive)
            continue;

        projectile.mActive = true;
        projectile.mPosition = inPosition;
        projectile.mVelocity = inVelocity;
        projectile.mDamage = inDamage;
        projectile.mLife = inLife;
        return;
    }
}


void DuskSwarmScript::UpdateEnemyProjectiles(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const Vec3 player = GetPlayerCenter();

    for (Array<EnemyProjectile>* pool : { &m_Arrows, &m_Orbs })
    {
        const bool orbs = pool == &m_Orbs;

        for (EnemyProjectile& projectile : *pool)
        {
            if (!projectile.mActive)
                continue;

            projectile.mPosition += projectile.mVelocity * inDeltaTime;
            projectile.mLife -= inDeltaTime;

            if (orbs && RandomFloat(0.0f, 1.0f) < 0.3f)
                Emit(projectile.mPosition, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_PURPLE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.6f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.1f, .mSizeMax = 0.18f });

            const Vec2 flat = Vec2(projectile.mPosition.x, projectile.mPosition.z);
            bool hit = false;

            if (glm::distance(flat, m_PlayerPosition) < projectile.mRadius + cPlayerRadius && glm::abs(projectile.mPosition.y - player.y) < 1.2f)
            {
                const bool dodged = m_DashTimer > -0.12f;

                if (!dodged)
                {
                    DamagePlayer(projectile.mDamage);
                    hit = true;
                }
            }

            const bool outside = glm::any(glm::greaterThan(glm::abs(flat), Vec2(cArenaHalfSize)));

            if (hit || outside || projectile.mLife <= 0.0f)
            {
                projectile.mActive = false;
                Park(projectile.mNode);

                if (hit || orbs)
                    Emit(projectile.mPosition, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = orbs ? FX_PURPLE : FX_SPARK, .mCount = 6, .mSpeedMin = 1.5f, .mSpeedMax = 4.0f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.12f, .mSizeMax = 0.2f, .mDrag = 3.0f });

                continue;
            }

            const float spin = orbs ? m_Time * 6.0f : 0.0f;
            const Quat rotation = LookRotation(projectile.mVelocity) * Quat(Vec3(0.0f, 0.0f, spin));
            SetTransform(projectile.mNode, projectile.mPosition, Vec3(orbs ? 1.0f + 0.15f * glm::sin(m_Time * 20.0f) : 1.0f), rotation);
        }
    }
}

} // RK
