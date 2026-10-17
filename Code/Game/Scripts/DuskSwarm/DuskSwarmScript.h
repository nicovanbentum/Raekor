#pragma once

#include "DuskSwarm.h"

namespace RK {

class DuskSwarmScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(DuskSwarmScript);

    enum EState
    {
        STATE_PLAYING,
        STATE_LEVEL_UP,
        STATE_GAME_OVER,
        STATE_VICTORY
    };

    using Node = MeshNodes::Node;

    struct RigPart
    {
        Node mNode = Node::Null;
        Vec3 mRest = Vec3(0.0f);
    };

    struct ModelMesh
    {
        Node mNode = Node::Null;
        Entity mMaterial = Entity::Null;
    };

    struct Model
    {
        Node mRoot = Node::Null;
        StaticArray<RigPart, RIG_PART_COUNT> mParts;
        Array<ModelMesh> mMeshes;
    };

    struct PartTemplate
    {
        String mName;
        Vec3 mJoint = Vec3(0.0f);
        Mesh mMesh;
    };

    struct Enemy
    {
        Model mModel;
        int mType = ENEMY_GHOUL;
        bool mActive = false;
        bool mDying = false;
        bool mElite = false;
        Vec2 mPosition = Vec2(0.0f);
        Vec2 mVelocity = Vec2(0.0f);
        Vec2 mKnockback = Vec2(0.0f);
        Vec2 mDashDirection = Vec2(0.0f);
        float mHealth = 0.0f;
        float mMaxHealth = 0.0f;
        float mFlashTimer = 0.0f;
        float mHitPunch = 0.0f;
        float mDeathTimer = 0.0f;
        float mSpawnTimer = 0.0f;
        float mBladeCooldown = 0.0f;
        float mAttackTimer = 0.0f;
        float mActionTimer = 0.0f;
        int mAction = ACTION_NONE;
        int mTelegraph = -1;
        int mAura = -1;
        float mPhase = 0.0f;
        float mYaw = 0.0f;
        float mWalkPhase = 0.0f;
        float mDeathSpin = 0.0f;
        bool mDeathImpact = false;
        StaticArray<Vec3, RIG_PART_COUNT> mShatterVelocity;
        StaticArray<Vec3, RIG_PART_COUNT> mShatterAxis;
    };

    struct Projectile
    {
        Node mNode = Node::Null;
        bool mActive = false;
        Vec2 mPosition = Vec2(0.0f);
        Vec2 mVelocity = Vec2(0.0f);
        float mLifetime = 0.0f;
        float mDamage = 0.0f;
        int mPierce = 0;
        int mLastHit = -1;
        float mHeight = 0.9f;
        bool mHoming = true;
    };

    struct Fireball
    {
        Node mNode = Node::Null;
        bool mActive = false;
        Vec3 mStart = Vec3(0.0f);
        Vec3 mTarget = Vec3(0.0f);
        Vec3 mPosition = Vec3(0.0f);
        float mTime = 0.0f;
        float mDuration = 0.0f;
        float mDamage = 0.0f;
        float mRadius = 0.0f;
    };

    struct EnemyProjectile
    {
        Node mNode = Node::Null;
        bool mActive = false;
        Vec3 mPosition = Vec3(0.0f);
        Vec3 mVelocity = Vec3(0.0f);
        float mLife = 0.0f;
        float mDamage = 0.0f;
        float mRadius = 0.3f;
    };

    struct Meteor
    {
        Node mNode = Node::Null;
        bool mActive = false;
        Vec3 mStart = Vec3(0.0f);
        Vec3 mTarget = Vec3(0.0f);
        float mTime = 0.0f;
        float mDuration = 1.0f;
        float mDamage = 0.0f;
        float mRadius = 0.0f;
    };

    struct Gem
    {
        Model mModel;
        bool mActive = false;
        bool mAttracted = false;
        Vec2 mPosition = Vec2(0.0f);
        float mSpeed = 0.0f;
        float mPhase = 0.0f;
        int mValue = 1;
    };

    struct Pickup
    {
        Model mModel;
        Node mGlow = Node::Null;
        int mType = PICKUP_POTION;
        bool mActive = false;
        bool mOpening = false;
        Vec2 mPosition = Vec2(0.0f);
        float mPhase = 0.0f;
        float mTime = 0.0f;
        float mOpenTimer = 0.0f;
    };

    struct Particle
    {
        Node mNode = Node::Null;
        Entity mMaterial = Entity::Null;
        bool mActive = false;
        bool mStretch = false;
        bool mBounce = false;
        Vec3 mPosition = Vec3(0.0f);
        Vec3 mVelocity = Vec3(0.0f);
        Vec3 mAxis = Vec3(0.0f, 1.0f, 0.0f);
        float mAngle = 0.0f;
        float mSpin = 0.0f;
        float mLife = 0.0f;
        float mMaxLife = 1.0f;
        float mSize = 0.1f;
        float mEndSize = 0.0f;
        float mGravity = 0.0f;
        float mDrag = 0.0f;
    };

    struct ParticleDesc
    {
        int    mMesh = PARTICLE_SPARK;
        int    mMaterial = FX_SPARK;
        int    mCount = 8;
        float  mSpeedMin = 2.0f;
        float  mSpeedMax = 6.0f;
        float  mLifeMin = 0.3f;
        float  mLifeMax = 0.6f;
        float  mSizeMin = 0.1f;
        float  mSizeMax = 0.2f;
        float  mEndSize = 0.0f;
        float  mGravity = 0.0f;
        float  mDrag = 2.0f;
        float  mUpBias = 0.0f;
        float  mSpread = 0.0f;
        bool   mFlat = false;
        Vec3   mBaseVelocity = Vec3(0.0f);
    };

    struct NovaBurst
    {
        StaticArray<Node, 3> mNodes = {};
        StaticArray<Entity, 3> mMaterials = {};
        bool mActive = false;
        Vec3 mPosition = Vec3(0.0f);
        float mRadius = 1.0f;
        float mRotation = 0.0f;
        float mLife = 0.0f;
    };

    struct Telegraph
    {
        Node mDisc = Node::Null;
        Node mRing = Node::Null;
        bool mActive = false;
        Vec3 mPosition = Vec3(0.0f);
        float mRadius = 1.0f;
        float mTime = 0.0f;
        float mDuration = 1.0f;
    };

    struct Beam
    {
        Node mNode = Node::Null;
        bool mActive = false;
        Vec3 mStart = Vec3(0.0f);
        Vec3 mEnd = Vec3(0.0f);
        float mWidth = 1.0f;
        float mLife = 0.0f;
        float mMaxLife = 0.15f;
    };

    struct FlashLight
    {
        Entity mEntity = Entity::Null;
        bool mActive = false;
        Vec3 mColor = Vec3(1.0f);
        float mIntensity = 0.0f;
        float mLife = 0.0f;
        float mMaxLife = 0.3f;
    };

    struct EliteAura
    {
        Node mNode = Node::Null;
        bool mActive = false;
    };

    struct Obstacle
    {
        Vec2 mPosition = Vec2(0.0f);
        float mRadius = 0.5f;
    };

    struct DamageNumber
    {
        Vec3 mPosition = Vec3(0.0f);
        float mTime = 0.0f;
        int mValue = 0;
        bool mCrit = false;
    };

    struct ChatMessage
    {
        String mText;
        Vec4 mColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    };

    struct XPDrop
    {
        int mValue = 0;
        float mTime = 0.0f;
    };

    struct Popup
    {
        String mText;
        String mSubText;
        float mTime = 0.0f;
        float mDuration = 2.2f;
        Vec4 mColor = Vec4(1.0f);
    };

    void OnStart() override;
    void OnStop() override;
    void OnUpdate(float inDeltaTime) override;
    void OnEvent(const SDL_Event& inEvent) override;
    void OnPausedUpdate(float inDeltaTime) override;
    void OnPausedEvent(const SDL_Event& inEvent) override;

private:
    Entity CreateEntity(StringView inName, Entity inParent = Entity::Null);
    Entity CreateMaterial(Vec3 inAlbedo, Vec3 inEmissive, float inRoughness, float inMetallic = 0.0f);
    void SetTransform(Entity inEntity, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f));
    void SetTransform(Node inNode, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f));
    void Park(Node inNode);
    void SetMaterial(Node inNode, Entity inMaterial);
    void SetModelMaterial(const Model& inModel, Entity inMaterial);
    void RestoreModelMaterials(const Model& inModel);
    void Pose(const Model& inModel, int inPart, const Vec3& inOffset, const Vec3& inEuler, float inScale = 1.0f);
    float RandomFloat(float inMin, float inMax);
    int RandomInt(int inMin, int inMax);
    Vec3 RandomDirection();
    Vec2 RandomCircle();
    static Vec3 ToWorld(const Vec2& inPosition, float inHeight);
    static float GetYaw(const Vec2& inDirection);
    static float LerpAngle(float inFrom, float inTo, float inT);
    static Quat LookRotation(const Vec3& inDirection);
    static float EaseOutCubic(float inT);
    static float EaseOutBack(float inT);
    static Vec2 ClampToArena(const Vec2& inPosition, float inRadius);
    static int GetRigPart(const String& inName);

    uint32_t LoadTexture(const char* inFile, bool inSRGB, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA);
    void LoadTextures();
    void ReleaseModels();
    void LoadModels();
    Array<Model> CreateModels(const String& inName, int inCount, Entity inMaterial = Entity::Null);
    Model CreateModel(const String& inName, Entity inMaterial = Entity::Null);
    void CreateMaterials();
    void CreateArena();
    void CreatePlayer();
    void CreatePools();
    void CreateCamera();

    Entity CreateMusic(StringView inName, const char* inFile, float inVolume);
    void CreateAudio();
    bool CanPlaySfx(const char* inFile, float inMinInterval);
    void PlaySfx(const char* inFile, float inVolume = 1.0f, float inPitch = 1.0f, float inMinInterval = 0.0f);
    void PlaySfxAt(const char* inFile, const Vec3& inPosition, float inVolume = 1.0f, float inPitch = 1.0f, float inMinInterval = 0.0f);
    void PlayEntitySound(Entity inEntity);
    void StopEntitySound(Entity inEntity);
    void PlayMusic(bool inBoss);
    void StopMusic();
    void UpdateSounds();

    void CreateSky();
    void RestoreSky();
    float GetSkyAngle() const;
    void UpdateSky();

    void DestroyRuntimeEntities();
    void NewGame();

    float GetMaxHealth() const { return cPlayerBaseHealth + 25.0f * m_UpgradeLevels[UPGRADE_VITALITY]; }
    float GetMoveSpeed() const { return cPlayerBaseSpeed * ( 1.0f + 0.12f * m_UpgradeLevels[UPGRADE_BOOTS] ); }
    float GetMagnetRadius() const { return cBaseMagnetRadius * ( 1.0f + 0.4f * m_UpgradeLevels[UPGRADE_MAGNET] ); }
    float GetDamageMultiplier() const { return 1.0f + 0.15f * m_UpgradeLevels[UPGRADE_POWER]; }
    float GetCritChance() const { return 0.06f + 0.03f * m_UpgradeLevels[UPGRADE_POWER]; }
    float GetCooldownMultiplier() const { return glm::pow(0.92f, float(m_UpgradeLevels[UPGRADE_HASTE])); }
    float GetAreaMultiplier() const { return 1.0f + 0.12f * m_UpgradeLevels[UPGRADE_AREA]; }
    int   GetExperienceToNextLevel() const { return 5 + ( m_Level - 1 ) * 4 + ( m_Level - 1 ) * ( m_Level - 1 ) / 3; }
    Vec3  GetPlayerCenter() const { return ToWorld(m_PlayerPosition, 0.9f); }
    Vec3  GetStaffTip() const { return m_FirstPerson ? m_WandTip : ToWorld(m_PlayerPosition, 1.75f) + Vec3(glm::cos(m_PlayerYaw) * 0.38f, 0.0f, -glm::sin(m_PlayerYaw) * 0.38f); }

    Vec2 GetInputDirection() const;

    Vec2 GetLookForward() const { return Vec2(-glm::sin(m_LookYaw), -glm::cos(m_LookYaw)); }
    Vec2 GetLookRight() const { return Vec2(glm::cos(m_LookYaw), -glm::sin(m_LookYaw)); }

    void ToggleFirstPerson();
    void TryDash();
    void UpdatePlayer(float inDeltaTime);
    Vec2 ResolveObstacles(Vec2 inPosition, float inRadius) const;
    void DamagePlayer(float inDamage);
    void AddTrauma(float inAmount, float inCap = 1.0f);
    void AddTraumaAt(const Vec3& inPosition, float inAmount);

    void UpdateSpawning(float inDeltaTime);
    Vec2 GetSpawnPosition();
    int SpawnEnemy(int inType, const Vec2& inPosition, bool inElite = false);
    void BuildGrid();
    static IVec2 GetGridCoord(const Vec2& inPosition);
    static int GetGridCell(const Vec2& inPosition);

    template<typename Fn>
    void ForEachEnemyNear(const Vec2& inPosition, float inRadius, Fn&& inFunction)
    {
        const IVec2 min_coord = GetGridCoord(inPosition - Vec2(inRadius + cMaxEnemyRadius));
        const IVec2 max_coord = GetGridCoord(inPosition + Vec2(inRadius + cMaxEnemyRadius));

        for (int y = min_coord.y; y <= max_coord.y; y++)
        {
            for (int x = min_coord.x; x <= max_coord.x; x++)
            {
                const int cell = x + y * cGridDim;

                for (int entry = m_GridStart[cell]; entry < m_GridStart[cell + 1]; entry++)
                {
                    const int index = m_GridEntries[entry];
                    Enemy& enemy = m_Enemies[index];

                    if (!enemy.mActive || enemy.mDying)
                        continue;

                    if (glm::distance(enemy.mPosition, inPosition) <= inRadius + cEnemyArchetypes[enemy.mType].mRadius)
                        inFunction(index, enemy);
                }
            }
        }
    }

    int FindNearestEnemy(const Vec2& inPosition, float inMaxDistance, int inExclude = -1, const Array<int>* inIgnore = nullptr);

    void UpdateEnemies(float inDeltaTime);
    void UpdateLich(Enemy& ioEnemy, float inDistance, const Vec2& inToPlayer, const Vec2& inSide, Vec2& outDesired, float inDeltaTime);
    void BruteSlam(Enemy& ioEnemy);
    void BloaterBurst(int inIndex);
    void Despawn(int inIndex);
    void ReleaseAura(Enemy& ioEnemy);
    bool DamageEnemy(int inIndex, float inDamage, const Vec2& inKnockback, bool inCanCrit = true);
    void KillEnemy(int inIndex);
    void UpdateDying(float inDeltaTime);
    void FireEnemyProjectile(Array<EnemyProjectile>& ioPool, const Vec3& inPosition, const Vec3& inVelocity, float inDamage, float inLife);
    void UpdateEnemyProjectiles(float inDeltaTime);

    void UpdateWeapons(float inDeltaTime);
    void FireBolts(int inLevel, float inDamageMultiplier);
    void UpdateProjectiles(float inDeltaTime);
    void LaunchFireball(float inDamage, float inRadius);
    void UpdateFireballs(float inDeltaTime);
    void CastLightning(int inJumps, float inDamage);
    void SpawnLightningArc(const Vec3& inFrom, const Vec3& inTo, float inWidth);
    void CallMeteor(float inDamage, float inRadius, float inDelay);
    void UpdateMeteors(float inDeltaTime);
    void Explode(const Vec3& inPosition, float inRadius, float inDamage, EExplosion inStyle, bool inHurtsPlayer, float inPlayerDamage);

    void SpawnGem(const Vec2& inPosition, int inValue);
    static int GetGemTier(int inValue);
    void UpdateGems(float inDeltaTime);
    void SpawnPickup(int inType, const Vec2& inPosition);
    void UpdatePickups(float inDeltaTime);
    void CollectPickup(Pickup& ioPickup);
    void OpenChest(const Vec2& inPosition);
    void AddExperience(int inValue);
    void UpdateProgress();
    void OpenLevelUp();
    void ApplyUpgrade(int inUpgrade);
    void ChooseUpgrade(int inChoice);

    void Emit(const Vec3& inPosition, const ParticleDesc& inDesc);
    void UpdateParticles(float inDeltaTime);
    void SpawnNovaBurst(const Vec3& inPosition, float inRadius);
    void UpdateNovaBursts(float inDeltaTime);
    int SpawnTelegraph(const Vec3& inPosition, float inRadius, float inDuration, Entity inMaterial);
    void ReleaseTelegraph(int inIndex);
    void ReleaseTelegraph(Telegraph& ioTelegraph);
    void UpdateTelegraphs(float inDeltaTime);
    void SpawnBeam(const Vec3& inStart, const Vec3& inEnd, float inWidth, float inLife);
    void UpdateBeams(float inDeltaTime);
    void SpawnLight(const Vec3& inPosition, const Vec3& inColor, float inIntensity, float inRadius, float inLife);
    void UpdateLights(float inDeltaTime);

    void AnimatePlayer(float inDeltaTime);
    void AnimateEnemy(Enemy& ioEnemy, float inDeltaTime);
    void AnimateDyingEnemy(Enemy& ioEnemy);
    void UpdateVisuals(float inDeltaTime);

    void UpdateCamera(float inDeltaTime);
    void UpdateWand();

    void AddDamageNumber(const Vec3& inPosition, int inValue, bool inCrit);
    void AddChat(const String& inText, const Vec4& inColor);
    void AddPopup(const String& inText, const String& inSubText, const Vec4& inColor);
    bool ProjectToScreen(const Vec3& inPosition, Vec2& outScreen) const;
    static Vec4 Fade(const Vec4& inColor, float inAlpha);

    static constexpr Vec4 cRSYellow = Vec4(1.0f, 1.0f, 0.0f, 1.0f);
    static constexpr Vec4 cRSOrange = Vec4(1.0f, 0.6f, 0.12f, 1.0f);
    static constexpr Vec4 cRSWhite = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    static constexpr Vec4 cRSGreen = Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    static constexpr Vec4 cRSRed = Vec4(1.0f, 0.1f, 0.1f, 1.0f);
    static constexpr Vec4 cHealthGreen = Vec4(0.0f, 0.78f, 0.0f, 1.0f);
    static constexpr Vec4 cHealthRed = Vec4(0.72f, 0.0f, 0.0f, 1.0f);
    static constexpr Vec4 cChatBlack = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    static constexpr Vec4 cChatRed = Vec4(0.55f, 0.0f, 0.0f, 1.0f);
    static constexpr Vec4 cChatBlue = Vec4(0.0f, 0.0f, 0.62f, 1.0f);
    static constexpr Vec4 cChatGreen = Vec4(0.0f, 0.4f, 0.0f, 1.0f);
    static constexpr Vec4 cChatPurple = Vec4(0.42f, 0.0f, 0.5f, 1.0f);

    void DrawLabel(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    void DrawStonePanel(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha = 1.0f);
    void DrawDivider(Vec2 inPos, float inWidth, float inScale, float inAlpha);
    void DrawWindow(Vec2 inPos, Vec2 inSize, StringView inTitle, float inScale, float inAlpha);
    void DrawFlatBar(Vec2 inPos, Vec2 inSize, float inFill, Vec4 inFillColor, Vec4 inBackColor, float inScale);
    void DrawSlotBox(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha = 1.0f);
    void DrawButton(Vec2 inPos, Vec2 inSize, StringView inLabel, bool inHovered, float inAlpha, float inScale);
    void DrawGem(Vec2 inCenter, float inSize, Vec4 inColor);
    void DrawUpgradeIcon(int inUpgrade, Vec2 inCenter, float inSize, float inAlpha);
    static Vec4 GetOrbValueColor(float inFraction);
    void DrawOrb(Vec2 inPos, float inScale, float inFill, const Vec3& inColor, uint32_t inIcon, StringView inValue, float inValueFraction);
    void DrawHUD(float inDeltaTime);
    void DrawCrosshair(const Vec2& inDisplay, float inScale);
    void DrawInfoBox(float inScale, float inMargin);
    void DrawExperience(const Vec2& inDisplay, float inDeltaTime, float inScale, float inMargin);
    void DrawBossBar(const Vec2& inDisplay, float inScale, float inMargin);
    float GetWeaponCooldownFraction(int inUpgrade) const;
    void DrawInventory(const Vec2& inDisplay, float inScale, float inMargin);
    void DrawChatbox(const Vec2& inDisplay, float inScale, float inMargin);
    void DrawHintArrows(const Vec2& inDisplay, float inScale);
    static String FormatTime(float inSeconds);
    void DrawHitsplats(float inDeltaTime, float inScale);
    void DrawPopups(const Vec2& inDisplay, float inDeltaTime, float inScale);
    void DrawIntro(const Vec2& inDisplay, float inScale);
    void DrawLevelUp(const Vec2& inDisplay, float inScale);
    void DrawEndScreen(const Vec2& inDisplay, float inScale);

    struct PauseLayout
    {
        float mScale = 1.0f;
        Vec2 mDisplay = Vec2(0.0f);
        Vec2 mPanelPos = Vec2(0.0f);
        Vec2 mPanelSize = Vec2(0.0f);
        Vec2 mSliderPos = Vec2(0.0f);
        Vec2 mSliderSize = Vec2(0.0f);
        Vec2 mSliderPadding = Vec2(0.0f);
        float mSliderStep = 0.0f;
        StaticArray<Vec2, PAUSE_TAB_COUNT> mTabPos = {};
        Vec2 mTabSize = Vec2(0.0f);
        Vec2 mCheckboxSize = Vec2(0.0f);
        Vec2 mGodModePos = Vec2(0.0f);
        Vec2 mSettingsPos = Vec2(0.0f);
        float mSettingStep = 0.0f;
        Vec2 mButtonPos = Vec2(0.0f);
        Vec2 mButtonSize = Vec2(0.0f);
        Vec2 mQuitPos = Vec2(0.0f);

        Vec2 GetSettingPos(int inSetting) const { return mSettingsPos + Vec2(0.0f, mSettingStep * float(inSetting)); }
        Vec2 GetSliderPos(int inSlider) const { return mSliderPos + Vec2(0.0f, mSliderStep * float(inSlider)); }
    };

    PauseLayout GetPauseLayout() const;
    static bool IsInside(const Vec2& inPoint, const Vec2& inPos, const Vec2& inSize);
    float GetMasterVolume();
    void SetMasterVolume(float inVolume);
    float GetSensitivityScale() const;
    void SetSensitivityScale(float inScale);
    float GetSliderValue(int inSlider);
    String GetSliderText(int inSlider);
    void SetSliderFromMouse(const PauseLayout& inLayout, int inSlider);
    void ToggleGodMode();
    static int* GetGraphicsSetting(int inSetting);
    void ToggleGraphicsSetting(int inSetting);
    void SetPauseTab(int inTab);
    void Resume();
    void Quit();
    void DrawPauseMenu();
    void DrawCheckbox(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inChecked, float inAlpha, float inScale);
    void DrawPauseGraphicsTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha);
    void DrawPauseGeneralTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha);

private:
    bool m_Started = false;
    EState m_State = STATE_PLAYING;
    float m_Time = 0.0f;
    float m_RunTime = 0.0f;
    float m_EndTime = 0.0f;
    float m_BestTime = 0.0f;
    std::mt19937 m_Random;

    uint32_t m_PaletteTexture = 0;
    uint32_t m_PaletteEmissiveTexture = 0;
    HashMap<String, Array<PartTemplate>> m_MeshTemplates;

    struct GroundTextures
    {
        uint32_t mAlbedo = 0;
        uint32_t mNormal = 0;
        uint32_t mRoughness = 0;
    };

    StaticArray<GroundTextures, GROUND_LAYER_COUNT> m_GroundTextures = {};
    StaticArray<Entity, GROUND_LAYER_COUNT> m_GroundMaterials;

    uint32_t m_PanelTexture = 0;
    uint32_t m_ParchmentTexture = 0;
    uint32_t m_SlotTexture = 0;
    uint32_t m_ButtonTexture = 0;
    uint32_t m_OrbFrameTexture = 0;
    uint32_t m_OrbFillTexture = 0;
    uint32_t m_HitsplatTexture = 0;
    uint32_t m_HitsplatMaxTexture = 0;
    uint32_t m_HintArrowTexture = 0;
    uint32_t m_HeartTexture = 0;
    uint32_t m_GemTexture = 0;
    uint32_t m_DashIconTexture = 0;
    StaticArray<uint32_t, UPGRADE_COUNT + 1> m_UpgradeIconTextures = {};
    int m_HudFont = 0;
    int m_TitleFont = 0;

    Entity m_RootEntity = Entity::Null;
    Entity m_CameraEntity = Entity::Null;
    Entity m_PlayerEntity = Entity::Null;
    Model m_PlayerModel;
    Model m_WandModel;
    Vec3 m_WandTip = Vec3(0.0f);
    bool m_AimHintShown = false;
    StaticArray<Node, cMaxBlades> m_Blades;

    MeshNodes m_Nodes;

    Entity m_SunEntity = Entity::Null;
    bool m_RestoreSun = false;
    Vec4 m_SavedSunColor = Vec4(1.0f);
    float m_SavedSunIlluminance = 0.0f;
    Quat m_SavedSunRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

    Entity m_ListenerEntity = Entity::Null;
    Entity m_MusicEntity = Entity::Null;
    Entity m_BossMusicEntity = Entity::Null;
    HashMap<const char*, float> m_SoundTimes;
    int m_GemStreak = 0;
    float m_LastGemTime = 0.0f;
    bool m_EndJinglePlayed = false;

    float m_PauseTime = 0.0f;
    int m_PauseTab = PAUSE_TAB_GENERAL;
    Vec2 m_MousePosition = Vec2(-1.0f);
    int m_DraggingSlider = SLIDER_NONE;

    Entity m_PaletteMaterial = Entity::Null;
    Entity m_FlashMaterial = Entity::Null;
    Entity m_EliteFlashMaterial = Entity::Null;
    Entity m_BoltMaterial = Entity::Null;
    Entity m_DangerMaterial = Entity::Null;
    Entity m_PoisonTelegraphMaterial = Entity::Null;
    StaticArray<Entity, 3> m_GemMaterials;
    StaticArray<Entity, FX_MATERIAL_COUNT> m_FXMaterials;

    Vec2 m_PlayerPosition = Vec2(0.0f);
    Vec2 m_PlayerVelocity = Vec2(0.0f);
    Vec2 m_FacingDirection = Vec2(0.0f, -1.0f);
    Vec2 m_DashDirection = Vec2(0.0f, -1.0f);
    float m_PlayerYaw = 0.0f;
    float m_PlayerWalkPhase = 0.0f;
    float m_PlayerDeathTimer = 0.0f;
    float m_Health = cPlayerBaseHealth;
    float m_InvulnerableTimer = 0.0f;
    float m_DashTimer = -1.0f;
    float m_DashCooldownTimer = 0.0f;
    float m_CastTimer = 0.0f;

    int m_Level = 1;
    int m_Experience = 0;
    int m_LichKills = 0;
    int m_PendingLevelUps = 0;
    int m_Kills = 0;
    int m_EliteKills = 0;
    int m_BossKills = 0;
    int m_Streak = 0;
    int m_BestStreak = 0;
    float m_StreakTimer = 0.0f;
    float m_DamageDealt = 0.0f;
    StaticArray<int, UPGRADE_COUNT> m_UpgradeLevels = {};

    StaticArray<int, cChoiceCount> m_Choices = {};
    int m_ChoiceCount = 0;
    int m_SelectedChoice = 0;
    float m_LevelUpTime = 0.0f;

    Array<Enemy> m_Enemies;
    StaticArray<Array<int>, ENEMY_TYPE_COUNT> m_FreeEnemies;
    Array<Projectile> m_Projectiles;
    Array<Fireball> m_Fireballs;
    Array<EnemyProjectile> m_Arrows;
    Array<EnemyProjectile> m_Orbs;
    Array<Meteor> m_Meteors;
    Array<Gem> m_Gems;
    Array<Pickup> m_Pickups;
    Array<Obstacle> m_Obstacles;
    Array<Vec2> m_Pushes;

    StaticArray<Array<Particle>, PARTICLE_MESH_COUNT> m_Particles;
    StaticArray<int, PARTICLE_MESH_COUNT> m_ParticleCursor = {};
    Array<NovaBurst> m_NovaBursts;
    Array<Telegraph> m_Telegraphs;
    Array<Beam> m_Beams;
    Array<FlashLight> m_Lights;
    Array<EliteAura> m_EliteAuras;

    Array<int> m_GridStart;
    Array<int> m_GridFill;
    Array<int> m_GridCells;
    Array<int> m_GridEntries;

    float m_SpawnAccumulator = 0.0f;
    float m_NextSwarmTime = cSwarmInterval;
    float m_NextBruteTime = cBruteInterval;
    int m_NextBoss = 0;
    int m_BossEnemy = -1;
    int m_BossPattern = 0;
    float m_BoltTimer = 0.0f;
    float m_NovaTimer = 0.0f;
    float m_FireballTimer = 0.0f;
    float m_LightningTimer = 0.0f;
    float m_MeteorTimer = 0.0f;
    float m_BladeAngle = 0.0f;

    Array<DamageNumber> m_DamageNumbers;
    Array<Popup> m_Popups;
    Array<ChatMessage> m_ChatMessages;
    Array<XPDrop> m_XPDrops;

    Vec3 m_CameraTarget = Vec3(0.0f);
    float m_CameraDistance = 22.0f;
    float m_CameraKick = 0.0f;
    bool m_FirstPerson = false;
    float m_LookYaw = 0.0f;
    float m_LookPitch = 0.0f;
    float m_Trauma = 0.0f;
    float m_HitStop = 0.0f;
    float m_HitFlash = 0.0f;
    float m_ScreenFlash = 0.0f;

    int* m_GodMode = nullptr;
    int* m_AutoPick = nullptr;
    float* m_StartTime = nullptr;
    int* m_StartWeapons = nullptr;
    float* m_ShakeScale = nullptr;
    float* m_SensitivityScale = nullptr;

public:
    DuskSwarmScript()
    {
        m_Blades.fill(Node::Null);
        m_GemMaterials.fill(Entity::Null);
        m_FXMaterials.fill(Entity::Null);
        m_GroundMaterials.fill(Entity::Null);
    }
};

} // RK
