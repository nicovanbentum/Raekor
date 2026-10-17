#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::UpdateSpawning(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const float minutes = m_RunTime / 60.0f;
    float spawn_rate = 1.2f + minutes * 1.7f + minutes * minutes * 0.3f;

    if (m_BossEnemy >= 0)
        spawn_rate *= 0.5f;

    m_SpawnAccumulator += spawn_rate * inDeltaTime;

    const StaticArray<float, 5> weights =
    {
        1.0f,
        m_RunTime > 45.0f ? 0.6f * glm::min(( m_RunTime - 45.0f ) / 120.0f, 1.0f) : 0.0f,
        m_RunTime > 90.0f ? 0.4f * glm::min(( m_RunTime - 90.0f ) / 120.0f, 1.0f) : 0.0f,
        m_RunTime > 135.0f ? 0.18f : 0.0f,
        m_RunTime > 180.0f ? 0.3f * glm::min(( m_RunTime - 180.0f ) / 120.0f, 1.0f) : 0.0f,
    };

    const float total_weight = weights[0] + weights[1] + weights[2] + weights[3] + weights[4];

    while (m_SpawnAccumulator >= 1.0f)
    {
        m_SpawnAccumulator -= 1.0f;

        float roll = RandomFloat(0.0f, total_weight);
        int type = ENEMY_GHOUL;

        for (int index = 0; index < 5; index++)
        {
            if (roll < weights[index])
            {
                type = index;
                break;
            }

            roll -= weights[index];
        }

        SpawnEnemy(type, GetSpawnPosition(), m_RunTime > 90.0f && RandomFloat(0.0f, 1.0f) < cEliteChance);
    }

    if (m_RunTime >= m_NextSwarmTime)
    {
        m_NextSwarmTime += cSwarmInterval;

        const int count = 24 + int(minutes * 4.0f);

        for (int index = 0; index < count; index++)
        {
            const float angle = glm::two_pi<float>() * float(index) / float(count);
            SpawnEnemy(ENEMY_BAT, ClampToArena(m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * 17.0f, 1.0f));
        }

        AddPopup("The Swarm Descends", "Bats close in from every side", Vec4(0.8f, 0.55f, 1.0f, 1.0f));
        AddChat("The swarm descends! Bats close in from every side.", cChatRed);
        PlaySfx(cSwarmSound, 0.7f);
    }

    if (m_RunTime >= m_NextBruteTime)
    {
        m_NextBruteTime += cBruteInterval;

        const int count = 1 + int(minutes / 3.0f);

        for (int index = 0; index < count; index++)
            SpawnEnemy(ENEMY_BRUTE, GetSpawnPosition(), m_RunTime > 180.0f && index == 0);

        AddChat(count > 1 ? "Brutes approach." : "A brute approaches.", cChatRed);
        PlaySfx(cBruteRoarSound, 0.6f, RandomFloat(0.95f, 1.05f));
    }

    if (m_NextBoss < int(cBossTimes.size()) && m_RunTime >= cBossTimes[m_NextBoss] && m_BossEnemy < 0)
    {
        m_NextBoss++;

        const Vec2 position = ClampToArena(m_PlayerPosition + RandomCircle() * 12.0f, 4.0f);
        const int index = SpawnEnemy(ENEMY_LICH, position);

        if (index >= 0)
        {
            m_BossEnemy = index;
            AddPopup("The Lich Rises", "Destroy it before it destroys you", Vec4(0.8f, 0.4f, 1.0f, 1.0f));
            AddChat("The Lich rises from its grave!", cChatRed);
            AddTrauma(0.6f);
            PlaySfx(cLichRiseSound, 0.9f);
            PlayMusic(true);
            SpawnLight(ToWorld(position, 2.0f), Vec3(0.75f, 0.35f, 1.0f), 4000000.0f, 18.0f, 1.2f);
            Emit(ToWorld(position, 0.5f), ParticleDesc { .mMesh = PARTICLE_CHUNK, .mMaterial = FX_DIRT, .mCount = 30, .mSpeedMin = 4.0f, .mSpeedMax = 10.0f, .mLifeMin = 0.8f, .mLifeMax = 1.4f, .mSizeMin = 0.15f, .mSizeMax = 0.35f, .mEndSize = 0.6f, .mGravity = 20.0f, .mDrag = 0.5f, .mUpBias = 1.2f, .mSpread = 1.0f });
        }
    }
}


Vec2 DuskSwarmScript::GetSpawnPosition()
{
    Vec2 best = ClampToArena(m_PlayerPosition + Vec2(cSpawnDistance, 0.0f), 1.0f);
    float best_distance = 0.0f;

    for (int attempt = 0; attempt < 8; attempt++)
    {
        const Vec2 position = ClampToArena(m_PlayerPosition + RandomCircle() * RandomFloat(cSpawnDistance, cSpawnDistance + 4.0f), 1.0f);
        const float distance = glm::distance(position, m_PlayerPosition);

        if (distance >= cMinSpawnDistance)
            return position;

        if (distance > best_distance)
        {
            best = position;
            best_distance = distance;
        }
    }

    return best;
}


int DuskSwarmScript::SpawnEnemy(int inType, const Vec2& inPosition, bool inElite)
{
    Array<int>& free_list = m_FreeEnemies[inType];

    if (free_list.empty())
        return -1;

    const int index = free_list.back();
    free_list.pop_back();

    Enemy& enemy = m_Enemies[index];
    const EnemyArchetype& archetype = cEnemyArchetypes[inType];

    const float health_scale = 1.0f + m_RunTime / 150.0f;

    enemy.mActive = true;
    enemy.mDying = false;
    enemy.mElite = inElite;
    enemy.mPosition = inPosition;
    enemy.mVelocity = Vec2(0.0f);
    enemy.mKnockback = Vec2(0.0f);
    enemy.mMaxHealth = archetype.mHealth * health_scale * ( inElite ? 6.0f : 1.0f );
    enemy.mHealth = enemy.mMaxHealth;
    enemy.mFlashTimer = 0.0f;
    enemy.mHitPunch = 0.0f;
    enemy.mDeathTimer = 0.0f;
    enemy.mSpawnTimer = archetype.mSpawnDuration;
    enemy.mBladeCooldown = 0.0f;
    enemy.mAttackTimer = RandomFloat(1.0f, 2.5f);
    enemy.mActionTimer = 0.0f;
    enemy.mAction = ACTION_NONE;
    enemy.mTelegraph = -1;
    enemy.mAura = -1;
    enemy.mPhase = RandomFloat(0.0f, glm::two_pi<float>());
    enemy.mWalkPhase = enemy.mPhase;
    enemy.mYaw = GetYaw(m_PlayerPosition - inPosition);
    enemy.mDeathImpact = false;

    RestoreModelMaterials(enemy.mModel);

    if (inElite)
    {
        for (int aura = 0; aura < cEliteAuraPoolSize; aura++)
        {
            if (!m_EliteAuras[aura].mActive)
            {
                m_EliteAuras[aura].mActive = true;
                enemy.mAura = aura;
                break;
            }
        }
    }

    if (archetype.mSpawnStyle == SPAWN_RISE && glm::distance(inPosition, m_PlayerPosition) < 30.0f)
    {
        Emit(ToWorld(inPosition, 0.1f), ParticleDesc { .mMesh = PARTICLE_CHUNK, .mMaterial = FX_DIRT, .mCount = inType == ENEMY_LICH ? 20 : 6, .mSpeedMin = 2.0f, .mSpeedMax = 4.5f, .mLifeMin = 0.5f, .mLifeMax = 0.9f, .mSizeMin = 0.08f, .mSizeMax = 0.18f, .mEndSize = 0.5f, .mGravity = 16.0f, .mDrag = 0.5f, .mUpBias = 1.5f, .mSpread = 0.4f });
    }

    return index;
}


void DuskSwarmScript::BuildGrid()
{
    PROFILE_FUNCTION_CPU();

    m_GridStart.assign(cGridDim * cGridDim + 1, 0);
    m_GridCells.resize(m_Enemies.size());

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        const Enemy& enemy = m_Enemies[index];
        m_GridCells[index] = enemy.mActive && !enemy.mDying ? GetGridCell(enemy.mPosition) : -1;

        if (m_GridCells[index] >= 0)
            m_GridStart[m_GridCells[index] + 1]++;
    }

    for (int cell = 0; cell < cGridDim * cGridDim; cell++)
        m_GridStart[cell + 1] += m_GridStart[cell];

    m_GridEntries.resize(m_GridStart.back());
    m_GridFill.assign(m_GridStart.begin(), m_GridStart.end() - 1);

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        if (m_GridCells[index] >= 0)
            m_GridEntries[m_GridFill[m_GridCells[index]]++] = int(index);
    }
}


IVec2 DuskSwarmScript::GetGridCoord(const Vec2& inPosition)
{
    const Vec2 local = ( inPosition + Vec2(cArenaHalfSize + 2.0f) ) / cGridCellSize;
    return glm::clamp(IVec2(glm::floor(local)), IVec2(0), IVec2(cGridDim - 1));
}


int DuskSwarmScript::GetGridCell(const Vec2& inPosition)
{
    const IVec2 coord = GetGridCoord(inPosition);
    return coord.x + coord.y * cGridDim;
}


int DuskSwarmScript::FindNearestEnemy(const Vec2& inPosition, float inMaxDistance, int inExclude, const Array<int>* inIgnore)
{
    int nearest = -1;
    float nearest_distance = inMaxDistance;

    for (size_t index = 0; index < m_Enemies.size(); index++)
    {
        const Enemy& enemy = m_Enemies[index];

        if (!enemy.mActive || enemy.mDying || int(index) == inExclude)
            continue;

        if (inIgnore && std::find(inIgnore->begin(), inIgnore->end(), int(index)) != inIgnore->end())
            continue;

        const float distance = glm::distance(enemy.mPosition, inPosition);

        if (distance < nearest_distance)
        {
            nearest = int(index);
            nearest_distance = distance;
        }
    }

    return nearest;
}

} // RK
