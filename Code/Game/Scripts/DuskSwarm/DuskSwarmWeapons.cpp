#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::UpdateWeapons(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const float damage_multiplier = GetDamageMultiplier();
    const float cooldown_multiplier = GetCooldownMultiplier();
    const float area = GetAreaMultiplier();

    if (const int level = m_UpgradeLevels[UPGRADE_BOLT]; level > 0)
    {
        m_BoltTimer -= inDeltaTime;

        const bool can_fire = m_FirstPerson ? m_Input->IsButtonDown(SDL_BUTTON_LEFT) : true;

        if (m_BoltTimer <= 0.0f && can_fire)
        {
            m_BoltTimer = ( 0.95f - 0.1f * ( level - 1 ) ) * cooldown_multiplier * ( m_FirstPerson ? cAimedBoltCooldownScale : 1.0f );
            FireBolts(level, damage_multiplier);
        }
    }

    if (const int level = m_UpgradeLevels[UPGRADE_BLADES]; level > 0)
    {
        m_BladeAngle += inDeltaTime * ( 2.6f + 0.25f * level );

        const int count = glm::min(1 + level, cMaxBlades);
        const float radius = ( 2.0f + 0.2f * level ) * glm::sqrt(area);
        const float damage = ( 2.0f + 0.75f * level ) * damage_multiplier;

        for (int blade = 0; blade < count; blade++)
        {
            const float angle = m_BladeAngle + glm::two_pi<float>() * float(blade) / float(count);
            const Vec2 position = m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * radius;

            ForEachEnemyNear(position, 0.4f, [&](int inIndex, Enemy& inEnemy)
            {
                if (inEnemy.mBladeCooldown > 0.0f)
                    return;

                inEnemy.mBladeCooldown = 0.45f;

                const Vec2 away = inEnemy.mPosition - m_PlayerPosition;
                DamageEnemy(inIndex, damage, glm::length(away) > 0.0001f ? glm::normalize(away) * 6.0f : Vec2(0.0f));
                PlaySfxAt(cBladeHitSound, ToWorld(position, 0.8f), 0.45f, RandomFloat(0.9f, 1.15f), 0.06f);
                Emit(ToWorld(position, 0.8f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_EMBER, .mCount = 4, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.12f, .mSizeMax = 0.2f, .mGravity = 6.0f, .mDrag = 2.0f });
            });
        }
    }

    if (const int level = m_UpgradeLevels[UPGRADE_NOVA]; level > 0)
    {
        m_NovaTimer -= inDeltaTime;

        if (m_NovaTimer <= 0.0f)
        {
            m_NovaTimer = ( 3.6f - 0.35f * ( level - 1 ) ) * cooldown_multiplier;

            const float radius = ( 3.5f + 0.6f * level ) * area;
            const float damage = ( 4.0f + 1.5f * level ) * damage_multiplier;

            ForEachEnemyNear(m_PlayerPosition, radius, [&](int inIndex, Enemy& inEnemy)
            {
                const Vec2 away = inEnemy.mPosition - m_PlayerPosition;
                DamageEnemy(inIndex, damage, glm::length(away) > 0.0001f ? glm::normalize(away) * 14.0f : Vec2(0.0f));
            });

            const Vec3 ground = ToWorld(m_PlayerPosition, 0.15f);
            PlaySfx(cNovaSound, 0.65f, RandomFloat(0.97f, 1.03f));
            SpawnNovaBurst(ground, radius);
            Emit(ground + Vec3(0.0f, 0.3f, 0.0f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_PURPLE, .mCount = 28, .mSpeedMin = radius * 2.0f, .mSpeedMax = radius * 3.0f, .mLifeMin = 0.25f, .mLifeMax = 0.4f, .mSizeMin = 0.25f, .mSizeMax = 0.4f, .mDrag = 4.0f, .mFlat = true });
            SpawnLight(ground + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.75f, 0.45f, 1.0f), 1500000.0f, radius * 2.5f, 0.35f);
            AddTrauma(0.08f, cAmbientTraumaCap);
        }
    }

    if (const int level = m_UpgradeLevels[UPGRADE_FIREBALL]; level > 0)
    {
        m_FireballTimer -= inDeltaTime;

        if (m_FireballTimer <= 0.0f)
        {
            m_FireballTimer = ( 2.6f - 0.25f * ( level - 1 ) ) * cooldown_multiplier;

            const int count = 1 + ( level >= 3 ) + ( level >= 5 );

            for (int shot = 0; shot < count; shot++)
                LaunchFireball(( 8.0f + 4.0f * level ) * damage_multiplier, ( 2.0f + 0.25f * level ) * area);
        }
    }

    if (const int level = m_UpgradeLevels[UPGRADE_LIGHTNING]; level > 0)
    {
        m_LightningTimer -= inDeltaTime;

        if (m_LightningTimer <= 0.0f)
        {
            m_LightningTimer = ( 2.2f - 0.2f * ( level - 1 ) ) * cooldown_multiplier;
            CastLightning(2 + level, ( 7.0f + 3.0f * level ) * damage_multiplier);
        }
    }

    if (const int level = m_UpgradeLevels[UPGRADE_METEOR]; level > 0)
    {
        m_MeteorTimer -= inDeltaTime;

        if (m_MeteorTimer <= 0.0f)
        {
            m_MeteorTimer = ( 5.5f - 0.5f * ( level - 1 ) ) * cooldown_multiplier;

            const int count = 1 + level / 2;

            for (int meteor = 0; meteor < count; meteor++)
                CallMeteor(( 25.0f + 10.0f * level ) * damage_multiplier, ( 3.0f + 0.3f * level ) * area, meteor * 0.25f);
        }
    }
}


void DuskSwarmScript::FireBolts(int inLevel, float inDamageMultiplier)
{
    const bool aimed = m_FirstPerson;
    const int target = aimed ? -1 : FindNearestEnemy(m_PlayerPosition, 18.0f);
    const Vec2 aim = aimed ? GetLookForward() : target >= 0 ? glm::normalize(m_Enemies[target].mPosition - m_PlayerPosition + Vec2(0.0001f)) : m_FacingDirection;
    const Vec3 tip = GetStaffTip();
    const float speed = aimed ? cAimedBoltSpeed : cBoltSpeed;

    const int count = 1 + ( inLevel >= 3 ) + ( inLevel >= 5 );
    const float spread = 0.18f;

    for (int bolt = 0; bolt < count; bolt++)
    {
        const float angle = ( float(bolt) - float(count - 1) * 0.5f ) * spread;
        const Vec2 direction = Vec2(aim.x * glm::cos(angle) - aim.y * glm::sin(angle), aim.x * glm::sin(angle) + aim.y * glm::cos(angle));

        for (Projectile& projectile : m_Projectiles)
        {
            if (projectile.mActive)
                continue;

            projectile.mActive = true;
            projectile.mPosition = aimed ? Vec2(tip.x, tip.z) : m_PlayerPosition + direction * 0.6f;
            projectile.mVelocity = direction * speed;
            projectile.mLifetime = aimed ? 1.2f : 1.4f;
            projectile.mHeight = aimed ? tip.y : 0.9f;
            projectile.mHoming = !aimed;
            projectile.mDamage = ( 3.0f + 1.0f * ( inLevel - 1 ) ) * inDamageMultiplier;
            projectile.mPierce = 1 + inLevel / 2;
            projectile.mLastHit = -1;
            break;
        }
    }

    m_CastTimer = 0.25f;
    PlaySfx(cBoltSound, 0.35f, RandomFloat(0.94f, 1.08f), 0.05f);
    const float spark_scale = aimed ? 0.25f : 1.0f;
    Emit(tip, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 4, .mSpeedMin = 1.0f * spark_scale, .mSpeedMax = 3.0f * spark_scale, .mLifeMin = 0.15f, .mLifeMax = 0.25f, .mSizeMin = 0.1f * spark_scale, .mSizeMax = 0.18f * spark_scale, .mDrag = 3.0f });
}


void DuskSwarmScript::UpdateProjectiles(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Projectile& projectile : m_Projectiles)
    {
        if (!projectile.mActive)
            continue;

        const int target = projectile.mHoming ? FindNearestEnemy(projectile.mPosition, 6.0f, projectile.mLastHit) : -1;

        if (target >= 0)
        {
            const Vec2 desired = glm::normalize(m_Enemies[target].mPosition - projectile.mPosition + Vec2(0.0001f)) * cBoltSpeed;
            projectile.mVelocity = glm::normalize(glm::mix(projectile.mVelocity, desired, 1.0f - glm::exp(-inDeltaTime * 4.0f))) * cBoltSpeed;
        }

        projectile.mPosition += projectile.mVelocity * inDeltaTime;
        projectile.mLifetime -= inDeltaTime;

        if (!projectile.mHoming)
            projectile.mHeight = glm::mix(projectile.mHeight, 1.0f, 1.0f - glm::exp(-inDeltaTime * 5.0f));

        if (RandomFloat(0.0f, 1.0f) < 0.5f)
            Emit(ToWorld(projectile.mPosition, projectile.mHeight),ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.8f, .mLifeMin = 0.12f, .mLifeMax = 0.22f, .mSizeMin = 0.1f, .mSizeMax = 0.15f, .mDrag = 2.0f });

        ForEachEnemyNear(projectile.mPosition, 0.2f, [&](int inIndex, Enemy& inEnemy)
        {
            if (projectile.mPierce <= 0 || inIndex == projectile.mLastHit)
                return;

            projectile.mLastHit = inIndex;
            projectile.mPierce--;

            DamageEnemy(inIndex, projectile.mDamage, glm::normalize(projectile.mVelocity) * 4.0f);
            PlaySfxAt(cBoltHitSound, ToWorld(projectile.mPosition, projectile.mHeight), 0.4f, RandomFloat(0.9f, 1.1f), 0.04f);
            Emit(ToWorld(projectile.mPosition, projectile.mHeight),ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 6, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mDrag = 3.0f });
        });

        const bool outside = glm::any(glm::greaterThan(glm::abs(projectile.mPosition), Vec2(cArenaHalfSize)));

        if (projectile.mPierce <= 0 || projectile.mLifetime <= 0.0f || outside)
        {
            projectile.mActive = false;
            Park(projectile.mNode);
        }
    }
}


void DuskSwarmScript::LaunchFireball(float inDamage, float inRadius)
{
    Array<int> candidates;

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        const Enemy& enemy = m_Enemies[index];

        if (enemy.mActive && !enemy.mDying && glm::distance(enemy.mPosition, m_PlayerPosition) < 14.0f)
            candidates.push_back(int(index));
    }

    Vec2 target = m_PlayerPosition + m_FacingDirection * 8.0f;

    if (!candidates.empty())
        target = m_Enemies[candidates[RandomInt(0, int(candidates.size()) - 1)]].mPosition;

    for (Fireball& fireball : m_Fireballs)
    {
        if (fireball.mActive)
            continue;

        fireball.mActive = true;
        fireball.mStart = GetStaffTip();
        fireball.mTarget = ToWorld(target, 0.3f);
        fireball.mPosition = fireball.mStart;
        fireball.mTime = 0.0f;
        fireball.mDuration = 0.45f + glm::distance(target, m_PlayerPosition) * 0.03f;
        fireball.mDamage = inDamage;
        fireball.mRadius = inRadius;
        m_CastTimer = 0.3f;
        PlaySfx(cFireballCastSound, 0.5f, RandomFloat(0.92f, 1.08f), 0.08f);
        return;
    }
}


void DuskSwarmScript::UpdateFireballs(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Fireball& fireball : m_Fireballs)
    {
        if (!fireball.mActive)
            continue;

        fireball.mTime += inDeltaTime;
        const float t = glm::min(fireball.mTime / fireball.mDuration, 1.0f);

        fireball.mPosition = glm::mix(fireball.mStart, fireball.mTarget, t) + Vec3(0.0f, 4.0f * t * ( 1.0f - t ) * 3.0f, 0.0f);

        Emit(fireball.mPosition, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_FIRE, .mCount = 2, .mSpeedMin = 0.2f, .mSpeedMax = 0.8f, .mLifeMin = 0.2f, .mLifeMax = 0.35f, .mSizeMin = 0.25f, .mSizeMax = 0.4f, .mDrag = 2.0f, .mSpread = 0.15f });

        if (RandomFloat(0.0f, 1.0f) < 0.5f)
            Emit(fireball.mPosition, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_SMOKE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.5f, .mLifeMin = 0.4f, .mLifeMax = 0.7f, .mSizeMin = 0.2f, .mSizeMax = 0.35f, .mEndSize = 1.6f, .mGravity = -1.5f, .mDrag = 1.0f });

        SetTransform(fireball.mNode, fireball.mPosition, Vec3(1.0f + 0.15f * glm::sin(m_Time * 30.0f)), Quat(Vec3(m_Time * 7.0f, m_Time * 5.0f, 0.0f)));

        if (t >= 1.0f)
        {
            fireball.mActive = false;
            Park(fireball.mNode);
            Explode(fireball.mTarget, fireball.mRadius, fireball.mDamage, EXPLOSION_FIRE, false, 0.0f);
        }
    }
}


void DuskSwarmScript::CastLightning(int inJumps, float inDamage)
{
    Array<int> hits;
    int current = FindNearestEnemy(m_PlayerPosition, 12.0f);

    if (current < 0)
        return;

    Vec3 from = GetStaffTip();

    for (int jump = 0; jump < inJumps && current >= 0; jump++)
    {
        hits.push_back(current);

        const Enemy& enemy = m_Enemies[current];
        const Vec3 to = ToWorld(enemy.mPosition, cEnemyArchetypes[enemy.mType].mTop * 0.6f);
        const Vec2 knock = glm::length(enemy.mPosition - m_PlayerPosition) > 0.001f ? glm::normalize(enemy.mPosition - m_PlayerPosition) * 3.0f : Vec2(0.0f);

        SpawnLightningArc(from, to, jump == 0 ? 1.4f : 1.0f);
        Emit(to, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_WHITE, .mCount = 8, .mSpeedMin = 3.0f, .mSpeedMax = 7.0f, .mLifeMin = 0.12f, .mLifeMax = 0.25f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mGravity = 4.0f, .mDrag = 3.0f });

        DamageEnemy(current, inDamage, knock);

        from = to;
        current = FindNearestEnemy(Vec2(to.x, to.z), 5.5f, -1, &hits);
    }

    SpawnLight(GetStaffTip(), Vec3(0.7f, 0.85f, 1.0f), 1200000.0f, 12.0f, 0.2f);
    AddTrauma(0.05f, cAmbientTraumaCap);
    PlaySfx(cLightningSound, 0.5f, RandomFloat(0.9f, 1.1f));
    m_CastTimer = 0.3f;
}


void DuskSwarmScript::SpawnLightningArc(const Vec3& inFrom, const Vec3& inTo, float inWidth)
{
    constexpr int cSegments = 6;

    Vec3 previous = inFrom;
    const Vec3 delta = inTo - inFrom;
    const Vec3 side = glm::normalize(glm::cross(delta + Vec3(0.0001f), Vec3(0.0f, 1.0f, 0.0f)) + Vec3(0.0001f));

    for (int segment = 1; segment <= cSegments; segment++)
    {
        const float t = float(segment) / float(cSegments);
        Vec3 point = inFrom + delta * t;

        if (segment < cSegments)
            point += side * RandomFloat(-0.45f, 0.45f) + Vec3(0.0f, RandomFloat(-0.3f, 0.3f), 0.0f);

        SpawnBeam(previous, point, inWidth, RandomFloat(0.12f, 0.2f));

        if (segment == cSegments / 2 && RandomFloat(0.0f, 1.0f) < 0.6f)
            SpawnBeam(point, point + RandomDirection() * RandomFloat(0.5f, 1.0f), inWidth * 0.6f, 0.12f);

        previous = point;
    }
}


void DuskSwarmScript::CallMeteor(float inDamage, float inRadius, float inDelay)
{
    Vec2 target = m_PlayerPosition + RandomCircle() * RandomFloat(4.0f, 12.0f);

    const int enemy = FindNearestEnemy(m_PlayerPosition + RandomCircle() * RandomFloat(3.0f, 10.0f), 16.0f);

    if (enemy >= 0)
        target = m_Enemies[enemy].mPosition;

    target = ClampToArena(target, 1.0f);

    for (Meteor& meteor : m_Meteors)
    {
        if (meteor.mActive)
            continue;

        meteor.mActive = true;
        meteor.mTarget = ToWorld(target, 0.0f);
        meteor.mStart = meteor.mTarget + Vec3(-5.0f, 16.0f, -4.0f);
        meteor.mTime = -inDelay;
        meteor.mDuration = 1.0f;
        meteor.mDamage = inDamage;
        meteor.mRadius = inRadius;
        return;
    }
}


void DuskSwarmScript::UpdateMeteors(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Meteor& meteor : m_Meteors)
    {
        if (!meteor.mActive)
            continue;

        meteor.mTime += inDeltaTime;

        if (meteor.mTime < 0.0f)
            continue;

        const float t = glm::min(meteor.mTime / meteor.mDuration, 1.0f);
        const Vec3 position = glm::mix(meteor.mStart, meteor.mTarget + Vec3(0.0f, 0.4f, 0.0f), t * t);

        SetTransform(meteor.mNode, position, Vec3(1.0f), Quat(Vec3(m_Time * 4.0f, m_Time * 3.0f, 0.0f)));

        Emit(position, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_FIRE, .mCount = 3, .mSpeedMin = 0.3f, .mSpeedMax = 1.0f, .mLifeMin = 0.25f, .mLifeMax = 0.45f, .mSizeMin = 0.4f, .mSizeMax = 0.7f, .mDrag = 2.0f, .mSpread = 0.3f });
        Emit(position, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = FX_SMOKE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.6f, .mLifeMin = 0.6f, .mLifeMax = 1.0f, .mSizeMin = 0.4f, .mSizeMax = 0.6f, .mEndSize = 2.0f, .mGravity = -1.0f, .mDrag = 1.0f, .mSpread = 0.3f });

        if (t >= 1.0f)
        {
            meteor.mActive = false;
            Park(meteor.mNode);
            Explode(meteor.mTarget + Vec3(0.0f, 0.3f, 0.0f), meteor.mRadius, meteor.mDamage, EXPLOSION_METEOR, false, 0.0f);
        }
    }
}


void DuskSwarmScript::Explode(const Vec3& inPosition, float inRadius, float inDamage, EExplosion inStyle, bool inHurtsPlayer, float inPlayerDamage)
{
    const Vec2 center = Vec2(inPosition.x, inPosition.z);

    if (inDamage > 0.0f)
    {
        ForEachEnemyNear(center, inRadius, [&](int inIndex, Enemy& inEnemy)
        {
            const Vec2 away = inEnemy.mPosition - center;
            const float distance = glm::length(away);
            const float falloff = glm::mix(1.0f, 0.5f, glm::clamp(distance / inRadius, 0.0f, 1.0f));
            DamageEnemy(inIndex, inDamage * falloff, distance > 0.0001f ? away / distance * 12.0f : Vec2(0.0f));
        });
    }

    if (inHurtsPlayer && glm::distance(center, m_PlayerPosition) < inRadius + cPlayerRadius)
        DamagePlayer(inPlayerDamage);

    int core = FX_FIRE, spark = FX_EMBER, chunk = FX_DIRT;
    Vec3 light = Vec3(1.0f, 0.55f, 0.2f);
    bool smoke = true;

    switch (inStyle)
    {
        case EXPLOSION_POISON: core = FX_POISON; spark = FX_POISON; chunk = FX_GOO;  light = Vec3(0.5f, 1.0f, 0.3f);  break;
        case EXPLOSION_ARCANE: core = FX_PURPLE; spark = FX_PURPLE; chunk = FX_BONE; light = Vec3(0.75f, 0.4f, 1.0f); smoke = false; break;
        case EXPLOSION_HOLY:   core = FX_WHITE;  spark = FX_GOLD;   chunk = FX_DIRT; light = Vec3(1.0f, 0.9f, 0.6f);  smoke = false; break;
        case EXPLOSION_DUST:   core = FX_DIRT;   spark = FX_DIRT;   chunk = FX_DIRT; light = Vec3(1.0f, 0.4f, 0.2f);  break;
        default: break;
    }

    const float scale = glm::clamp(inRadius / 2.5f, 0.6f, 2.5f);
    const int count = int(10.0f * scale);

    const char* sound = cExplosionSound;
    float volume = glm::clamp(0.4f + 0.15f * scale, 0.4f, 0.75f);
    float pitch = RandomFloat(0.9f, 1.1f);

    switch (inStyle)
    {
        case EXPLOSION_POISON: sound = cPoisonBurstSound;  volume = 0.65f; break;
        case EXPLOSION_ARCANE: sound = cExplosionSound;    volume = 0.35f; pitch *= 1.5f; break;
        case EXPLOSION_HOLY:   sound = cHolyBlastSound;    volume = 1.0f;  break;
        case EXPLOSION_METEOR: sound = cMeteorImpactSound; volume = 0.85f; break;
        case EXPLOSION_DUST:   sound = cSlamSound;         volume = 0.75f; break;
        default: break;
    }

    PlaySfxAt(sound, inPosition, volume, pitch, 0.05f);

    if (inStyle != EXPLOSION_DUST)
        Emit(inPosition, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = core, .mCount = count, .mSpeedMin = inRadius * 1.2f, .mSpeedMax = inRadius * 2.4f, .mLifeMin = 0.2f, .mLifeMax = 0.4f, .mSizeMin = 0.4f * scale, .mSizeMax = 0.8f * scale, .mEndSize = 0.2f, .mDrag = 5.0f, .mUpBias = 0.3f });

    Emit(inPosition, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = spark, .mCount = count + 6, .mSpeedMin = 6.0f, .mSpeedMax = 14.0f * scale, .mLifeMin = 0.3f, .mLifeMax = 0.7f, .mSizeMin = 0.18f, .mSizeMax = 0.3f, .mGravity = 12.0f, .mDrag = 1.5f, .mUpBias = 0.8f });
    Emit(inPosition, ParticleDesc { .mMesh = PARTICLE_CHUNK, .mMaterial = chunk, .mCount = count / 2 + 2, .mSpeedMin = 3.0f, .mSpeedMax = 8.0f * scale, .mLifeMin = 0.7f, .mLifeMax = 1.2f, .mSizeMin = 0.1f, .mSizeMax = 0.2f * scale, .mEndSize = 0.5f, .mGravity = 18.0f, .mDrag = 0.5f, .mUpBias = 1.4f });

    if (smoke)
        Emit(inPosition, ParticleDesc { .mMesh = PARTICLE_PUFF, .mMaterial = inStyle == EXPLOSION_DUST ? FX_DIRT : FX_SMOKE, .mCount = count / 2 + 2, .mSpeedMin = 0.8f, .mSpeedMax = 2.5f * scale, .mLifeMin = 0.7f, .mLifeMax = 1.3f, .mSizeMin = 0.4f * scale, .mSizeMax = 0.7f * scale, .mEndSize = 1.8f, .mGravity = -1.2f, .mDrag = 2.0f, .mUpBias = 0.5f, .mSpread = inRadius * 0.3f });

    SpawnLight(inPosition + Vec3(0.0f, 1.0f, 0.0f), light, 900000.0f * scale * scale, inRadius * 4.0f, 0.35f);
    AddTraumaAt(inPosition, glm::clamp(inRadius * 0.04f, 0.04f, 0.2f));
    m_CameraKick = glm::max(m_CameraKick, glm::min(inRadius * 0.08f, cMaxCameraKick));
}

} // RK
