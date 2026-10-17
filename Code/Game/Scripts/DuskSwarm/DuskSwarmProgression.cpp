#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::SpawnGem(const Vec2& inPosition, int inValue)
{
    Gem* target = nullptr;

    for (Gem& gem : m_Gems)
    {
        if (!gem.mActive)
        {
            target = &gem;
            break;
        }
    }

    if (target == nullptr)
    {
        float farthest = -1.0f;

        for (Gem& gem : m_Gems)
        {
            const float distance = glm::distance(gem.mPosition, m_PlayerPosition);

            if (!gem.mAttracted && distance > farthest)
            {
                target = &gem;
                farthest = distance;
            }
        }

        if (target == nullptr)
            return;

        AddExperience(target->mValue);
    }

    target->mActive = true;
    target->mAttracted = false;
    target->mPosition = ClampToArena(inPosition, 0.3f);
    target->mSpeed = 0.0f;
    target->mPhase = RandomFloat(0.0f, glm::two_pi<float>());
    target->mValue = inValue;

    SetModelMaterial(target->mModel, m_GemMaterials[GetGemTier(inValue)]);
}


int DuskSwarmScript::GetGemTier(int inValue)
{
    return inValue >= 25 ? 2 : inValue >= 5 ? 1 : 0;
}


void DuskSwarmScript::UpdateGems(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    const float magnet_radius = GetMagnetRadius();

    for (Gem& gem : m_Gems)
    {
        if (!gem.mActive)
            continue;

        const Vec2 delta = m_PlayerPosition - gem.mPosition;
        const float distance = glm::length(delta);

        if (distance < magnet_radius)
            gem.mAttracted = true;

        if (gem.mAttracted)
        {
            gem.mSpeed = glm::min(gem.mSpeed + inDeltaTime * 40.0f, 30.0f);
            gem.mPosition += ( distance > 0.0001f ? delta / distance : Vec2(0.0f) ) * glm::min(gem.mSpeed * inDeltaTime, distance);
        }

        if (glm::distance(gem.mPosition, m_PlayerPosition) < 0.6f)
        {
            gem.mActive = false;
            Park(gem.mModel.mRoot);
            AddExperience(gem.mValue);

            const int tier = GetGemTier(gem.mValue);

            m_GemStreak = m_Time - m_LastGemTime < cGemStreakTime ? ( m_GemStreak + 1 ) % ( cGemStreakSteps + 1 ) : 0;
            m_LastGemTime = m_Time;
            PlaySfx(cGemSound, 0.25f + 0.15f * tier, glm::pow(2.0f, float(m_GemStreak) / 12.0f) * ( tier == 2 ? 0.75f : 1.0f ), 0.04f);

            Emit(GetPlayerCenter(), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = tier == 2 ? FX_DANGER : tier == 1 ? FX_ARCANE : FX_HEAL, .mCount = 2 + tier * 3, .mSpeedMin = 1.0f, .mSpeedMax = 3.0f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.1f, .mSizeMax = 0.15f, .mDrag = 3.0f, .mUpBias = 0.5f });
        }
    }
}


void DuskSwarmScript::SpawnPickup(int inType, const Vec2& inPosition)
{
    for (Pickup& pickup : m_Pickups)
    {
        if (pickup.mActive || pickup.mType != inType)
            continue;

        pickup.mActive = true;
        pickup.mOpening = false;
        pickup.mPosition = ClampToArena(inPosition, 0.6f);
        pickup.mPhase = RandomFloat(0.0f, glm::two_pi<float>());
        pickup.mTime = 0.0f;
        pickup.mOpenTimer = 0.0f;

        return;
    }
}


void DuskSwarmScript::UpdatePickups(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    for (Pickup& pickup : m_Pickups)
    {
        if (!pickup.mActive)
            continue;

        pickup.mTime += inDeltaTime;

        if (pickup.mOpening)
        {
            pickup.mOpenTimer += inDeltaTime;

            if (RandomFloat(0.0f, 1.0f) < 0.6f)
                Emit(ToWorld(pickup.mPosition, 0.5f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_GOLD, .mCount = 2, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.4f, .mLifeMax = 0.8f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mGravity = 3.0f, .mDrag = 1.0f, .mUpBias = 3.0f, .mSpread = 0.2f });

            if (pickup.mOpenTimer >= 1.0f)
            {
                pickup.mActive = false;
                Park(pickup.mModel.mRoot);
                Park(pickup.mGlow);
                OpenChest(pickup.mPosition);
            }

            continue;
        }

        const float distance = glm::distance(pickup.mPosition, m_PlayerPosition);

        if (pickup.mType != PICKUP_CHEST && distance < 1.8f && distance > 0.0001f)
            pickup.mPosition += ( m_PlayerPosition - pickup.mPosition ) / distance * glm::min(distance, 9.0f * inDeltaTime);

        if (distance < 0.9f)
            CollectPickup(pickup);
    }
}


void DuskSwarmScript::CollectPickup(Pickup& ioPickup)
{
    const Vec3 center = GetPlayerCenter();

    switch (ioPickup.mType)
    {
        case PICKUP_POTION:
        {
            const float heal = GetMaxHealth() * 0.35f;
            m_Health = glm::min(GetMaxHealth(), m_Health + heal);
            AddChat(std::format("You drink the potion. It heals {} hitpoints.", int(heal)), cChatBlack);
            PlaySfx(cPotionSound, 0.7f);
            Emit(center, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_HEAL, .mCount = 24, .mSpeedMin = 0.5f, .mSpeedMax = 1.5f, .mLifeMin = 0.6f, .mLifeMax = 1.0f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mGravity = -4.0f, .mDrag = 1.0f, .mSpread = 0.8f, .mFlat = true });
        } break;

        case PICKUP_MAGNET:
        {
            for (Gem& gem : m_Gems)
                gem.mAttracted |= gem.mActive;

            AddChat("The magnet pulls every gem towards you.", cChatBlack);
            PlaySfx(cMagnetSound, 0.6f);
        } break;

        case PICKUP_BOMB:
        {
            Explode(center, 13.0f * GetAreaMultiplier(), 300.0f * GetDamageMultiplier(), EXPLOSION_HOLY, false, 0.0f);
            m_ScreenFlash = 1.0f;
            m_HitStop = 0.1f;
            AddTrauma(0.7f);
            AddPopup("Holy Bomb", "", Vec4(1.0f, 0.9f, 0.6f, 1.0f));
            AddChat("A holy blast purges the area!", cChatBlack);
        } break;

        case PICKUP_CHEST:
        {
            ioPickup.mOpening = true;
            ioPickup.mOpenTimer = 0.0f;
            PlaySfx(cChestOpenSound, 0.75f);
            SpawnLight(ToWorld(ioPickup.mPosition, 1.5f), Vec3(1.0f, 0.8f, 0.3f), 2000000.0f, 10.0f, 1.0f);
            return;
        }
    }

    ioPickup.mActive = false;
    Park(ioPickup.mModel.mRoot);
    Park(ioPickup.mGlow);
}


void DuskSwarmScript::OpenChest(const Vec2& inPosition)
{
    const int rewards = 1 + ( RandomFloat(0.0f, 1.0f) < 0.45f ) + ( RandomFloat(0.0f, 1.0f) < 0.15f );

    for (int reward = 0; reward < rewards; reward++)
    {
        Array<int> options;

        for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
        {
            if (m_UpgradeLevels[upgrade] > 0 && m_UpgradeLevels[upgrade] < cMaxUpgradeLevel)
                options.push_back(upgrade);
        }

        const int upgrade = options.empty() ? UPGRADE_FEAST : options[RandomInt(0, int(options.size()) - 1)];
        ApplyUpgrade(upgrade);
    }

    Emit(ToWorld(inPosition, 0.5f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_GOLD, .mCount = 40, .mSpeedMin = 4.0f, .mSpeedMax = 10.0f, .mLifeMin = 0.6f, .mLifeMax = 1.2f, .mSizeMin = 0.2f, .mSizeMax = 0.35f, .mGravity = 10.0f, .mDrag = 0.8f, .mUpBias = 1.5f });
    AddTrauma(0.25f);
    PlaySfx(cTreasureSound, 0.8f);
    AddPopup("Treasure!", rewards > 1 ? std::format("{} upgrades", rewards) : String("1 upgrade"), Vec4(1.0f, 0.82f, 0.35f, 1.0f));
    AddChat(std::format("You open the chest and find {} upgrade{}!", rewards, rewards > 1 ? "s" : ""), cChatBlack);
}


void DuskSwarmScript::AddExperience(int inValue)
{
    m_Experience += inValue;

    if (!m_XPDrops.empty() && m_XPDrops.back().mTime < 0.12f)
        m_XPDrops.back().mValue += inValue;
    else
        m_XPDrops.push_back(XPDrop { .mValue = inValue });

    while (m_Experience >= GetExperienceToNextLevel())
    {
        m_Experience -= GetExperienceToNextLevel();
        m_Level++;
        m_PendingLevelUps++;
        AddChat(std::format("Congratulations, you just advanced a level! You are now level {}.", m_Level), cChatBlue);
    }
}


void DuskSwarmScript::UpdateProgress()
{
    PROFILE_FUNCTION_CPU();

    if (m_RunTime >= cVictoryTime)
    {
        m_State = STATE_VICTORY;
        m_EndTime = m_Time;
        m_PlayerDeathTimer = 0.0f;
        m_BestTime = glm::max(m_BestTime, m_RunTime);
        m_ScreenFlash = 0.6f;

        StopMusic();
        PlaySfx(cVictorySound, 0.9f);
        return;
    }

    if (m_PendingLevelUps > 0)
    {
        m_PendingLevelUps--;
        OpenLevelUp();
    }
}


void DuskSwarmScript::OpenLevelUp()
{
    Array<int> options;

    for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
    {
        if (m_UpgradeLevels[upgrade] < cMaxUpgradeLevel)
            options.push_back(upgrade);
    }

    std::shuffle(options.begin(), options.end(), m_Random);

    m_ChoiceCount = 0;

    for (int option : options)
    {
        if (m_ChoiceCount == cChoiceCount)
            break;

        m_Choices[m_ChoiceCount++] = option;
    }

    if (m_ChoiceCount == 0)
        m_Choices[m_ChoiceCount++] = UPGRADE_FEAST;

    m_SelectedChoice = 0;
    m_LevelUpTime = m_Time;
    m_State = STATE_LEVEL_UP;

    PlaySfx(cLevelUpSound, 0.7f);

    const Vec3 ground = ToWorld(m_PlayerPosition, 0.1f);
    SpawnLight(GetPlayerCenter(), Vec3(1.0f, 0.85f, 0.5f), 1500000.0f, 10.0f, 0.6f);
    Emit(ground, ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_GOLD, .mCount = 30, .mSpeedMin = 3.0f, .mSpeedMax = 7.0f, .mLifeMin = 0.5f, .mLifeMax = 0.9f, .mSizeMin = 0.2f, .mSizeMax = 0.3f, .mDrag = 1.0f, .mUpBias = 4.0f, .mSpread = 0.6f });
}


void DuskSwarmScript::ApplyUpgrade(int inUpgrade)
{
    if (inUpgrade == UPGRADE_FEAST)
    {
        m_Health = glm::min(GetMaxHealth(), m_Health + GetMaxHealth() * 0.3f);
        AddChat("You eat the feast. It heals some health.", cChatBlack);
        return;
    }

    m_UpgradeLevels[inUpgrade]++;

    if (inUpgrade == UPGRADE_VITALITY)
        m_Health = glm::min(GetMaxHealth(), m_Health + 25.0f);

    if (inUpgrade == UPGRADE_NOVA && m_UpgradeLevels[inUpgrade] == 1)
        m_NovaTimer = 0.5f;

    AddChat(m_UpgradeLevels[inUpgrade] == 1 ? std::format("You have unlocked {}.", cUpgrades[inUpgrade].mName) : std::format("Your {} is now level {}.", cUpgrades[inUpgrade].mName, m_UpgradeLevels[inUpgrade]), cChatBlue);
}


void DuskSwarmScript::ChooseUpgrade(int inChoice)
{
    if (inChoice < 0 || inChoice >= m_ChoiceCount)
        return;

    ApplyUpgrade(m_Choices[inChoice]);
    PlaySfx(cChooseSound, 0.6f);

    m_ChoiceCount = 0;
    m_State = STATE_PLAYING;
}

} // RK
