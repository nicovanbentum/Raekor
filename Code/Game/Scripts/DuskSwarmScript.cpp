#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"
#include "../Engine/Profiler.h"
#include "../Engine/GLTF.h"

namespace RK {

static constexpr float cArenaHalfSize = 40.0f;
static constexpr float cPlayerRadius = 0.45f;
static constexpr float cPlayerBaseSpeed = 6.0f;
static constexpr float cPlayerBaseHealth = 100.0f;
static constexpr float cPlayerInvulnerability = 0.6f;
static constexpr float cBaseMagnetRadius = 2.5f;
static constexpr float cSpawnDistance = 26.0f;
static constexpr float cMinSpawnDistance = 15.0f;
static constexpr float cFlashDuration = 0.08f;
static constexpr float cVictoryTime = 600.0f;
static constexpr float cSwarmInterval = 90.0f;
static constexpr float cBruteInterval = 60.0f;
static constexpr float cDashSpeed = 26.0f;
static constexpr float cDashDuration = 0.2f;
static constexpr float cDashCooldown = 2.2f;
static constexpr float cMaxEnemyRadius = 1.3f;
static constexpr float cEliteChance = 0.035f;
static constexpr float cStreakWindow = 2.5f;
static constexpr float cXPDropDuration = 1.4f;
static constexpr int cChatLines = 6;
static constexpr float cAmbientTraumaCap = 0.25f;
static constexpr float cMaxCameraKick = 0.35f;

static constexpr StaticArray<float, 2> cBossTimes = { 270.0f, 540.0f };

static constexpr float cGridCellSize = 2.0f;
static constexpr int   cGridDim = int(( cArenaHalfSize * 2.0f + 4.0f ) / cGridCellSize) + 1;

static constexpr int cProjectilePoolSize = 80;
static constexpr int cFireballPoolSize = 16;
static constexpr int cArrowPoolSize = 60;
static constexpr int cOrbPoolSize = 140;
static constexpr int cGemPoolSize = 300;
static constexpr int cMeteorPoolSize = 12;
static constexpr int cNovaBurstPoolSize = 4;
static constexpr float cNovaBurstLife = 0.5f;
static constexpr int cTelegraphPoolSize = 24;
static constexpr int cBeamPoolSize = 120;
static constexpr int cLightPoolSize = 6;
static constexpr int cEliteAuraPoolSize = 8;
static constexpr int cPickupsPerType = 4;
static constexpr int cMaxBlades = 6;
static constexpr int cMaxUpgradeLevel = 5;
static constexpr int cMaxDamageNumbers = 80;
static constexpr int cChoiceCount = 3;

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -60.0f, 0.0f);

static constexpr const char* cRuntimeRootName = "DuskSwarm Runtime";
static constexpr const char* cModelFile = "Assets/DuskSwarm/DuskSwarm.glb";
static constexpr const char* cPaletteFile = "Assets/DuskSwarm/Palette.png";
static constexpr const char* cPaletteEmissiveFile = "Assets/DuskSwarm/PaletteEmissive.png";
static constexpr const char* cHudFontFile = "Assets/Fonts/PixelifySans.ttf";
static constexpr const char* cTitleFontFile = "Assets/Fonts/CinzelDecorative-Bold.ttf";

static constexpr uint8_t cNormalMapSwizzleAG = 0b11'10'01'11;

static constexpr const char* cMusicSound = "Assets/DuskSwarm/Audio/Music.ogg";
static constexpr const char* cBossMusicSound = "Assets/DuskSwarm/Audio/BossMusic.ogg";
static constexpr const char* cAmbienceSound = "Assets/DuskSwarm/Audio/Ambience.ogg";
static constexpr const char* cBoltSound = "Assets/DuskSwarm/Audio/Bolt.ogg";
static constexpr const char* cBoltHitSound = "Assets/DuskSwarm/Audio/BoltHit.ogg";
static constexpr const char* cBladeHitSound = "Assets/DuskSwarm/Audio/BladeHit.ogg";
static constexpr const char* cEnemyHitSound = "Assets/DuskSwarm/Audio/EnemyHit.ogg";
static constexpr const char* cNovaSound = "Assets/DuskSwarm/Audio/Nova.ogg";
static constexpr const char* cFireballCastSound = "Assets/DuskSwarm/Audio/FireballCast.ogg";
static constexpr const char* cExplosionSound = "Assets/DuskSwarm/Audio/Explosion.ogg";
static constexpr const char* cMeteorImpactSound = "Assets/DuskSwarm/Audio/MeteorImpact.ogg";
static constexpr const char* cLightningSound = "Assets/DuskSwarm/Audio/Lightning.ogg";
static constexpr const char* cPoisonBurstSound = "Assets/DuskSwarm/Audio/PoisonBurst.ogg";
static constexpr const char* cFuseSound = "Assets/DuskSwarm/Audio/Fuse.ogg";
static constexpr const char* cHolyBlastSound = "Assets/DuskSwarm/Audio/HolyBlast.ogg";
static constexpr const char* cSlamSound = "Assets/DuskSwarm/Audio/Slam.ogg";
static constexpr const char* cBruteRoarSound = "Assets/DuskSwarm/Audio/BruteRoar.ogg";
static constexpr const char* cArrowShotSound = "Assets/DuskSwarm/Audio/ArrowShot.ogg";
static constexpr const char* cLichCastSound = "Assets/DuskSwarm/Audio/LichCast.ogg";
static constexpr const char* cLichRiseSound = "Assets/DuskSwarm/Audio/LichRise.ogg";
static constexpr const char* cSwarmSound = "Assets/DuskSwarm/Audio/Swarm.ogg";
static constexpr const char* cPlayerHurtSound = "Assets/DuskSwarm/Audio/PlayerHurt.ogg";
static constexpr const char* cPlayerDeathSound = "Assets/DuskSwarm/Audio/PlayerDeath.ogg";
static constexpr const char* cDashSound = "Assets/DuskSwarm/Audio/Dash.ogg";
static constexpr const char* cGemSound = "Assets/DuskSwarm/Audio/Gem.ogg";
static constexpr const char* cLevelUpSound = "Assets/DuskSwarm/Audio/LevelUp.ogg";
static constexpr const char* cSelectSound = "Assets/DuskSwarm/Audio/Select.ogg";
static constexpr const char* cChooseSound = "Assets/DuskSwarm/Audio/Choose.ogg";
static constexpr const char* cPotionSound = "Assets/DuskSwarm/Audio/Potion.ogg";
static constexpr const char* cMagnetSound = "Assets/DuskSwarm/Audio/Magnet.ogg";
static constexpr const char* cChestOpenSound = "Assets/DuskSwarm/Audio/ChestOpen.ogg";
static constexpr const char* cTreasureSound = "Assets/DuskSwarm/Audio/Treasure.ogg";
static constexpr const char* cVictorySound = "Assets/DuskSwarm/Audio/Victory.ogg";
static constexpr const char* cGameOverSound = "Assets/DuskSwarm/Audio/GameOver.ogg";
static constexpr const char* cGhoulDeathSound = "Assets/DuskSwarm/Audio/GhoulDeath.ogg";
static constexpr const char* cBatDeathSound = "Assets/DuskSwarm/Audio/BatDeath.ogg";
static constexpr const char* cBoneShatterSound = "Assets/DuskSwarm/Audio/BoneShatter.ogg";
static constexpr const char* cWraithDeathSound = "Assets/DuskSwarm/Audio/WraithDeath.ogg";
static constexpr const char* cBruteDeathSound = "Assets/DuskSwarm/Audio/BruteDeath.ogg";
static constexpr const char* cLichDeathSound = "Assets/DuskSwarm/Audio/LichDeath.ogg";

static constexpr StaticArray<const char*, 37> cSoundEffects =
{
    cBoltSound, cBoltHitSound, cBladeHitSound, cEnemyHitSound, cNovaSound, cFireballCastSound, cExplosionSound,
    cMeteorImpactSound, cLightningSound, cPoisonBurstSound, cFuseSound, cHolyBlastSound, cSlamSound, cBruteRoarSound,
    cArrowShotSound, cLichCastSound, cLichRiseSound, cSwarmSound, cPlayerHurtSound, cPlayerDeathSound, cDashSound,
    cGemSound, cLevelUpSound, cSelectSound, cChooseSound, cPotionSound, cMagnetSound, cChestOpenSound,
    cTreasureSound, cVictorySound, cGameOverSound, cGhoulDeathSound, cBatDeathSound, cBoneShatterSound,
    cWraithDeathSound, cBruteDeathSound, cLichDeathSound
};

static constexpr float cListenerHeight = 6.0f;
static constexpr float cSoundMinDistance = 8.0f;
static constexpr float cGemStreakTime = 0.35f;
static constexpr int   cGemStreakSteps = 12;
static constexpr float cGameOverJingleDelay = 1.6f;
static constexpr float cVolumeStep = 0.05f;


enum EPauseTab
{
    PAUSE_TAB_GENERAL,
    PAUSE_TAB_GRAPHICS,
    PAUSE_TAB_COUNT
};

static constexpr StaticArray<const char*, PAUSE_TAB_COUNT> cPauseTabNames = { "General", "Graphics" };


struct GraphicsSetting
{
    const char* mCVar;
    const char* mLabel;
};

static constexpr StaticArray<GraphicsSetting, 5> cGraphicsSettings =
{
    GraphicsSetting { "r_enable_taa",     "Temporal anti-aliasing" },
    GraphicsSetting { "r_enable_ddgi",    "Global illumination" },
    GraphicsSetting { "r_enable_gtao",    "Ambient occlusion" },
    GraphicsSetting { "r_gtao_half_res",  "Half resolution AO" },
    GraphicsSetting { "r_enable_shadows", "Ray traced shadows" },
};


enum EEnemyType
{
    ENEMY_GHOUL,
    ENEMY_BAT,
    ENEMY_SKELETON,
    ENEMY_BLOATER,
    ENEMY_WRAITH,
    ENEMY_BRUTE,
    ENEMY_LICH,
    ENEMY_TYPE_COUNT
};


enum ESpawnStyle
{
    SPAWN_RISE,
    SPAWN_SWOOP,
    SPAWN_FADE
};


enum EDeathStyle
{
    DEATH_FALL,
    DEATH_SHATTER,
    DEATH_SPIN,
    DEATH_POP,
    DEATH_DISSIPATE,
    DEATH_BOSS
};


enum EEnemyAction
{
    ACTION_NONE,
    ACTION_AIM,
    ACTION_SLAM,
    ACTION_FUSE,
    ACTION_DASH_WINDUP,
    ACTION_DASH,
    ACTION_CAST
};


struct EnemyArchetype
{
    const char* mName;
    int   mPoolSize;
    float mRadius;
    float mSpeed;
    float mHealth;
    float mDamage;
    int   mGemValue;
    float mHover;
    float mTop;
    float mKnockback;
    float mSpawnDuration;
    float mDeathDuration;
    ESpawnStyle mSpawnStyle;
    EDeathStyle mDeathStyle;
    bool  mFlying;
};


static const StaticArray<EnemyArchetype, ENEMY_TYPE_COUNT> cEnemyArchetypes =
{
    EnemyArchetype { "Ghoul",    160, 0.45f, 2.6f, 5.0f,    8.0f,  1,  0.0f,  1.3f, 1.0f,  0.8f, 0.9f,  SPAWN_RISE,  DEATH_FALL,      false },
    EnemyArchetype { "Bat",      90,  0.35f, 4.6f, 3.0f,    5.0f,  1,  1.2f,  1.5f, 1.2f,  0.7f, 0.7f,  SPAWN_SWOOP, DEATH_SPIN,      true  },
    EnemyArchetype { "Skeleton", 50,  0.4f,  2.4f, 7.0f,    6.0f,  2,  0.0f,  1.5f, 1.0f,  0.8f, 1.1f,  SPAWN_RISE,  DEATH_SHATTER,   false },
    EnemyArchetype { "Bloater",  30,  0.7f,  1.7f, 14.0f,   10.0f, 2,  0.0f,  1.5f, 0.5f,  0.8f, 0.18f, SPAWN_RISE,  DEATH_POP,       false },
    EnemyArchetype { "Wraith",   40,  0.4f,  3.9f, 9.0f,    9.0f,  2,  0.25f, 1.9f, 0.8f,  0.6f, 0.7f,  SPAWN_FADE,  DEATH_DISSIPATE, true  },
    EnemyArchetype { "Brute",    14,  0.95f, 1.8f, 70.0f,   16.0f, 5,  0.0f,  2.6f, 0.15f, 1.0f, 1.2f,  SPAWN_RISE,  DEATH_FALL,      false },
    EnemyArchetype { "Lich",     1,   1.2f,  1.6f, 2600.0f, 20.0f, 25, 0.35f, 3.6f, 0.0f,  2.0f, 2.4f,  SPAWN_RISE,  DEATH_BOSS,      true  },
};

static constexpr StaticArray<const char*, ENEMY_TYPE_COUNT> cDeathSounds =
{
    cGhoulDeathSound, cBatDeathSound, cBoneShatterSound, nullptr, cWraithDeathSound, cBruteDeathSound, cLichDeathSound
};


enum EUpgrade
{
    UPGRADE_BOLT,
    UPGRADE_BLADES,
    UPGRADE_NOVA,
    UPGRADE_FIREBALL,
    UPGRADE_LIGHTNING,
    UPGRADE_METEOR,
    UPGRADE_BOOTS,
    UPGRADE_VITALITY,
    UPGRADE_MAGNET,
    UPGRADE_REGEN,
    UPGRADE_POWER,
    UPGRADE_HASTE,
    UPGRADE_AREA,
    UPGRADE_COUNT,
    UPGRADE_FEAST = UPGRADE_COUNT
};


struct UpgradeInfo
{
    const char* mName;
    const char* mDescription;
    bool mIsWeapon;
    Vec3 mColor;
};


static const StaticArray<UpgradeInfo, UPGRADE_COUNT + 1> cUpgrades =
{
    UpgradeInfo { "Arcane Bolt",     "Homing bolts seek the\nnearest foe",           true,  Vec3(0.35f, 0.85f, 1.0f) },
    UpgradeInfo { "Orbiting Blades", "Blades circle you and\ncut through foes",      true,  Vec3(1.0f, 0.62f, 0.2f) },
    UpgradeInfo { "Nova",            "A shockwave that hurls\nfoes away",            true,  Vec3(0.78f, 0.45f, 1.0f) },
    UpgradeInfo { "Fireball",        "Lobs fireballs that\nexplode on impact",       true,  Vec3(1.0f, 0.5f, 0.15f) },
    UpgradeInfo { "Chain Lightning", "Lightning arcs between\nnearby foes",          true,  Vec3(0.72f, 0.88f, 1.0f) },
    UpgradeInfo { "Meteor Storm",    "Calls meteors down\nfrom the sky",             true,  Vec3(1.0f, 0.32f, 0.18f) },
    UpgradeInfo { "Swift Boots",     "+12% movement speed",                          false, Vec3(0.45f, 1.0f, 0.6f) },
    UpgradeInfo { "Vitality",        "+25 max health\nand heal 25",                  false, Vec3(1.0f, 0.35f, 0.4f) },
    UpgradeInfo { "Magnet",          "+40% pickup range",                            false, Vec3(0.35f, 1.0f, 0.85f) },
    UpgradeInfo { "Regeneration",    "+0.6 health per second",                       false, Vec3(1.0f, 0.55f, 0.7f) },
    UpgradeInfo { "Power",           "+15% damage and\n+3% critical chance",         false, Vec3(1.0f, 0.82f, 0.3f) },
    UpgradeInfo { "Haste",           "-8% weapon cooldowns",                         false, Vec3(0.55f, 0.75f, 1.0f) },
    UpgradeInfo { "Area",            "+12% size of all\nareas of effect",            false, Vec3(0.9f, 0.6f, 1.0f) },
    UpgradeInfo { "Feast",           "Heal 30% of your\nmax health",                 false, Vec3(1.0f, 0.75f, 0.45f) },
};


static constexpr StaticArray<const char*, UPGRADE_COUNT + 1> cUpgradeIcons =
{
    "ArcaneBolt", "OrbitingBlades", "Nova", "Fireball", "ChainLightning", "MeteorStorm", "SwiftBoots",
    "Vitality", "Magnet", "Regeneration", "Power", "Haste", "Area", "Feast"
};


enum EPickup
{
    PICKUP_POTION,
    PICKUP_MAGNET,
    PICKUP_BOMB,
    PICKUP_CHEST,
    PICKUP_COUNT
};


static constexpr StaticArray<const char*, PICKUP_COUNT> cPickupModels = { "Potion", "Magnet", "Bomb", "Chest" };
static const StaticArray<Vec3, PICKUP_COUNT> cPickupColors = { Vec3(1.0f, 0.3f, 0.3f), Vec3(0.4f, 0.8f, 1.0f), Vec3(1.0f, 0.6f, 0.2f), Vec3(1.0f, 0.82f, 0.3f) };


enum EExplosion
{
    EXPLOSION_FIRE,
    EXPLOSION_POISON,
    EXPLOSION_ARCANE,
    EXPLOSION_HOLY,
    EXPLOSION_METEOR,
    EXPLOSION_DUST
};


enum EFXMaterial
{
    FX_FIRE,
    FX_EMBER,
    FX_SPARK,
    FX_ARCANE,
    FX_PURPLE,
    FX_POISON,
    FX_GOLD,
    FX_DANGER,
    FX_HEAL,
    FX_WHITE,
    FX_SMOKE,
    FX_DIRT,
    FX_BONE,
    FX_GOO,
    FX_FLESH,
    FX_MATERIAL_COUNT
};


enum EParticleMesh
{
    PARTICLE_SPARK,
    PARTICLE_CHUNK,
    PARTICLE_PUFF,
    PARTICLE_MESH_COUNT
};


enum EGroundLayer
{
    GROUND_GRASS,
    GROUND_DIRT,
    GROUND_STONE,
    GROUND_MOSS,
    GROUND_LAYER_COUNT
};


struct GroundLayer
{
    const char* mPart;
    const char* mTextures;
    Vec3 mTint;
};


static const StaticArray<GroundLayer, GROUND_LAYER_COUNT> cGroundLayers =
{
    GroundLayer { "Grass", "Grass", Vec3(1.0f) },
    GroundLayer { "Dirt",  "Dirt",  Vec3(1.0f) },
    GroundLayer { "Stone", "Stone", Vec3(1.0f) },
    GroundLayer { "Moss",  "Grass", Vec3(1.25f, 1.3f, 1.1f) },
};


static constexpr StaticArray<int, PARTICLE_MESH_COUNT> cParticlePoolSizes = { 320, 160, 220 };
static constexpr StaticArray<const char*, PARTICLE_MESH_COUNT> cParticleModels = { "Spark", "Chunk", "Puff" };


enum ERigPart
{
    RIG_BODY,
    RIG_HEAD,
    RIG_ARM_L,
    RIG_ARM_R,
    RIG_LEG_L,
    RIG_LEG_R,
    RIG_WING_L,
    RIG_WING_R,
    RIG_PART_COUNT
};


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

    enum class Node : uint32_t
    {
        Null = UINT32_MAX
    };

    struct SceneNode
    {
        Node mParent = Node::Null;
        uint32_t mBatch = UINT32_MAX;
        uint32_t mInstance = 0;
        Entity mMaterial = Entity::Null;
        Mat4x4 mLocal = Mat4x4(1.0f);
        Mat4x4 mWorld = Mat4x4(1.0f);
        bool mHidden = false;
        bool mVisible = false;
    };

    struct MeshBatch
    {
        Entity mEntity = Entity::Null;
        const MeshGeometry* mGeometry = nullptr;
    };

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

    void OnStart() override
    {
        DestroyRuntimeEntities();

        m_Random.seed(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

        m_RootEntity = m_Scene->CreateSpatialEntity(cRuntimeRootName);

        m_GodMode = &g_CVariables->Create("ds_god", 0, true);
        m_AutoPick = &g_CVariables->Create("ds_autopick", 0, true);
        m_StartTime = &g_CVariables->Create("ds_start_time", 0.0f, true);
        m_StartWeapons = &g_CVariables->Create("ds_weapons", 0, true);
        m_ShakeScale = &g_CVariables->Create("ds_shake", 1.0f, true);

        LoadTextures();
        LoadModels();
        CreateMaterials();
        CreateArena();
        CreatePlayer();
        CreatePools();
        CreateCamera();
        CreateAudio();

        NewGame();

        m_Started = true;
    }

    void OnStop() override
    {
        DestroyRuntimeEntities();

        m_Started = false;
    }

    void OnUpdate(float inDeltaTime) override
    {
        PROFILE_FUNCTION_CPU();

        if (!m_Started)
            OnStart();

        float delta_time = glm::min(inDeltaTime, 0.05f);

        m_Time += delta_time;
        m_PauseTime = 0.0f;
        m_DraggingVolume = false;

        if (m_HitStop > 0.0f)
        {
            m_HitStop -= delta_time;
            delta_time *= 0.08f;
        }

        if (m_State == STATE_LEVEL_UP && *m_AutoPick && m_Time - m_LevelUpTime > 1.5f)
            ChooseUpgrade(RandomInt(0, m_ChoiceCount - 1));

        if (m_State == STATE_PLAYING)
        {
            m_RunTime += delta_time;

            UpdatePlayer(delta_time);
            UpdateSpawning(delta_time);
            BuildGrid();
            UpdateEnemies(delta_time);
            UpdateWeapons(delta_time);
            UpdateProjectiles(delta_time);
            UpdateFireballs(delta_time);
            UpdateMeteors(delta_time);
            UpdateEnemyProjectiles(delta_time);
            UpdateGems(delta_time);
            UpdatePickups(delta_time);
            UpdateProgress();
        }
        else if (m_State == STATE_GAME_OVER || m_State == STATE_VICTORY)
        {
            m_PlayerDeathTimer += delta_time;
            BuildGrid();
        }

        if (m_State != STATE_LEVEL_UP)
        {
            UpdateDying(delta_time);
            UpdateParticles(delta_time);
            UpdateNovaBursts(delta_time);
            UpdateTelegraphs(delta_time);
            UpdateBeams(delta_time);
            UpdateLights(delta_time);
            UpdateVisuals(delta_time);
        }

        UpdateCamera(inDeltaTime);
        UpdateSounds();
        UpdateNodes();
        DrawHUD(inDeltaTime);
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (!m_Started)
            return;

        if (inEvent.type == SDL_EVENT_MOUSE_WHEEL)
            m_CameraDistance = glm::clamp(m_CameraDistance - inEvent.wheel.y * 1.5f, 14.0f, 34.0f);

        if (inEvent.type != SDL_EVENT_KEY_DOWN || inEvent.key.repeat)
            return;

        const SDL_Keycode key = inEvent.key.key;

        if (m_State == STATE_GAME_OVER || m_State == STATE_VICTORY)
        {
            if (key == SDLK_RETURN && m_PlayerDeathTimer > 1.0f)
                NewGame();

            return;
        }

        if (m_State == STATE_PLAYING)
        {
            if (key == SDLK_SPACE || key == SDLK_LSHIFT)
                TryDash();

            return;
        }

        if (m_State != STATE_LEVEL_UP || m_ChoiceCount == 0 || m_Time - m_LevelUpTime < 0.35f)
            return;

        switch (key)
        {
            case SDLK_1: ChooseUpgrade(0); break;
            case SDLK_2: ChooseUpgrade(1); break;
            case SDLK_3: ChooseUpgrade(2); break;
            case SDLK_LEFT:
            case SDLK_A: m_SelectedChoice = ( m_SelectedChoice + m_ChoiceCount - 1 ) % m_ChoiceCount; PlaySfx(cSelectSound, 0.5f); break;
            case SDLK_RIGHT:
            case SDLK_D: m_SelectedChoice = ( m_SelectedChoice + 1 ) % m_ChoiceCount; PlaySfx(cSelectSound, 0.5f); break;
            case SDLK_SPACE:
            case SDLK_RETURN: ChooseUpgrade(m_SelectedChoice); break;
        }
    }

    void OnPausedUpdate(float inDeltaTime) override
    {
        if (!m_Started)
            return;

        m_PauseTime += inDeltaTime;

        DrawHUD(0.0f);
        DrawPauseMenu();
    }

    void OnPausedEvent(const SDL_Event& inEvent) override
    {
        if (!m_Started)
            return;

        const PauseLayout layout = GetPauseLayout();

        switch (inEvent.type)
        {
            case SDL_EVENT_MOUSE_MOTION:
            {
                m_MousePosition = Vec2(inEvent.motion.x, inEvent.motion.y);

                if (m_DraggingVolume)
                    SetVolumeFromMouse(layout);
            } break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                if (inEvent.button.button != SDL_BUTTON_LEFT)
                    break;

                m_MousePosition = Vec2(inEvent.button.x, inEvent.button.y);

                for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
                {
                    if (IsInside(m_MousePosition, layout.mTabPos[tab], layout.mTabSize))
                        SetPauseTab(tab);
                }

                if (m_PauseTab == PAUSE_TAB_GENERAL)
                {
                    if (IsInside(m_MousePosition, layout.mSliderPos - layout.mSliderPadding, layout.mSliderSize + layout.mSliderPadding * 2.0f))
                    {
                        m_DraggingVolume = true;
                        SetVolumeFromMouse(layout);
                    }
                    else if (IsInside(m_MousePosition, layout.mGodModePos, layout.mCheckboxSize))
                    {
                        ToggleGodMode();
                    }
                }
                else
                {
                    for (int setting = 0; setting < int(cGraphicsSettings.size()); setting++)
                    {
                        if (IsInside(m_MousePosition, layout.GetSettingPos(setting), layout.mCheckboxSize))
                            ToggleGraphicsSetting(setting);
                    }
                }

                if (IsInside(m_MousePosition, layout.mButtonPos, layout.mButtonSize))
                    Resume();
                else if (IsInside(m_MousePosition, layout.mQuitPos, layout.mButtonSize))
                    Quit();
            } break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                if (inEvent.button.button == SDL_BUTTON_LEFT)
                    m_DraggingVolume = false;
            } break;

            case SDL_EVENT_KEY_DOWN:
            {
                switch (inEvent.key.key)
                {
                    case SDLK_LEFT:
                    case SDLK_A: SetMasterVolume(GetMasterVolume() - cVolumeStep); break;
                    case SDLK_RIGHT:
                    case SDLK_D: SetMasterVolume(GetMasterVolume() + cVolumeStep); break;
                    case SDLK_RETURN:
                    case SDLK_SPACE: if (!inEvent.key.repeat) Resume(); break;
                    case SDLK_Q: if (!inEvent.key.repeat) Quit(); break;
                    case SDLK_G: if (!inEvent.key.repeat) ToggleGodMode(); break;
                    case SDLK_TAB: if (!inEvent.key.repeat) SetPauseTab(( m_PauseTab + 1 ) % PAUSE_TAB_COUNT); break;
                }
            } break;
        }
    }

private:
    Entity CreateEntity(StringView inName, Entity inParent = Entity::Null)
    {
        const Entity entity = m_Scene->CreateSpatialEntity(inName);
        m_Scene->ParentTo(entity, inParent != Entity::Null ? inParent : m_RootEntity);
        return entity;
    }

    Entity CreateMaterial(Vec3 inAlbedo, Vec3 inEmissive, float inRoughness, float inMetallic = 0.0f)
    {
        const Entity entity = CreateEntity("Material");

        Material& material = m_Scene->Add<Material>(entity);
        material.albedo = Vec4(inAlbedo, 1.0f);
        material.emissive = inEmissive;
        material.roughness = inRoughness;
        material.metallic = inMetallic;

        return entity;
    }

    uint32_t GetMeshBatch(const Mesh& inMesh)
    {
        const MeshGeometry* geometry = inMesh.GetGeometry();

        if (geometry == nullptr)
            return UINT32_MAX;

        for (uint32_t batch = 0; batch < m_MeshBatches.size(); batch++)
        {
            if (m_MeshBatches[batch].mGeometry == geometry)
                return batch;
        }

        const Entity entity = CreateEntity(geometry->name);

        MeshInstanced& mesh_instanced = m_Scene->Add<MeshInstanced>(entity);
        mesh_instanced.SetAsset(inMesh.asset, inMesh.index);
        mesh_instanced.material = m_PaletteMaterial;

        m_App->GetRenderInterface()->UploadMeshBuffers(*mesh_instanced.asset);

        m_MeshBatches.push_back(MeshBatch { .mEntity = entity, .mGeometry = geometry });
        return uint32_t(m_MeshBatches.size() - 1);
    }

    Node CreateNode(Node inParent = Node::Null)
    {
        m_Nodes.emplace_back().mParent = inParent;
        return Node(m_Nodes.size() - 1);
    }

    Node CreateMeshNode(const Mesh& inMesh, Entity inMaterial, Node inParent)
    {
        const uint32_t batch = GetMeshBatch(inMesh);
        const Node node = CreateNode(inParent);

        SceneNode& scene_node = m_Nodes[uint32_t(node)];
        scene_node.mMaterial = inMaterial;

        if (batch != UINT32_MAX)
        {
            MeshInstanced& mesh_instanced = m_Scene->Get<MeshInstanced>(m_MeshBatches[batch].mEntity);
            scene_node.mBatch = batch;
            scene_node.mInstance = uint32_t(mesh_instanced.instances.size());
            mesh_instanced.instances.emplace_back().material = inMaterial;
        }

        return node;
    }

    void UpdateNodes()
    {
        PROFILE_FUNCTION_CPU();

        m_MeshBatchComponents.resize(m_MeshBatches.size());

        for (size_t batch = 0; batch < m_MeshBatches.size(); batch++)
            m_MeshBatchComponents[batch] = FindComponent<MeshInstanced>(m_MeshBatches[batch].mEntity);

        for (SceneNode& node : m_Nodes)
        {
            if (node.mParent == Node::Null)
            {
                node.mWorld = node.mLocal;
                node.mVisible = !node.mHidden;
            }
            else
            {
                const SceneNode& parent = m_Nodes[uint32_t(node.mParent)];
                node.mWorld = parent.mWorld * node.mLocal;
                node.mVisible = parent.mVisible && !node.mHidden;
            }

            if (node.mBatch == UINT32_MAX || m_MeshBatchComponents[node.mBatch] == nullptr)
                continue;

            MeshInstanced::Instance& instance = m_MeshBatchComponents[node.mBatch]->instances[node.mInstance];
            instance.transform = node.mWorld;
            instance.material = node.mMaterial;
            instance.visible = node.mVisible;
        }
    }

    static Mat4x4 ComposeTransform(const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation)
    {
        return glm::scale(glm::translate(Mat4x4(1.0f), inPosition) * glm::toMat4(inRotation), inScale);
    }

    void SetTransform(Entity inEntity, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f))
    {
        if (Transform* transform = FindComponent<Transform>(inEntity))
        {
            transform->position = inPosition;
            transform->rotation = inRotation;
            transform->scale = inScale;
            transform->Compose();
        }
    }

    void SetTransform(Node inNode, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f))
    {
        if (inNode == Node::Null)
            return;

        SceneNode& node = m_Nodes[uint32_t(inNode)];
        node.mLocal = ComposeTransform(inPosition, inScale, inRotation);
        node.mHidden = false;
    }

    void Park(Node inNode)
    {
        if (inNode != Node::Null)
            m_Nodes[uint32_t(inNode)].mHidden = true;
    }

    void SetMaterial(Node inNode, Entity inMaterial)
    {
        if (inNode != Node::Null && m_Nodes[uint32_t(inNode)].mBatch != UINT32_MAX)
            m_Nodes[uint32_t(inNode)].mMaterial = inMaterial;
    }

    void SetModelMaterial(const Model& inModel, Entity inMaterial)
    {
        for (const ModelMesh& mesh : inModel.mMeshes)
            SetMaterial(mesh.mNode, inMaterial);
    }

    void RestoreModelMaterials(const Model& inModel)
    {
        for (const ModelMesh& mesh : inModel.mMeshes)
            SetMaterial(mesh.mNode, mesh.mMaterial);
    }

    void Pose(const Model& inModel, int inPart, const Vec3& inOffset, const Vec3& inEuler, float inScale = 1.0f)
    {
        const RigPart& part = inModel.mParts[inPart];

        if (part.mNode != Node::Null)
            SetTransform(part.mNode, part.mRest + inOffset, Vec3(inScale), Quat(inEuler));
    }

    float RandomFloat(float inMin, float inMax)
    {
        return std::uniform_real_distribution<float>(inMin, inMax)(m_Random);
    }

    int RandomInt(int inMin, int inMax)
    {
        return std::uniform_int_distribution<int>(inMin, inMax)(m_Random);
    }

    Vec3 RandomDirection()
    {
        const float z = RandomFloat(-1.0f, 1.0f);
        const float angle = RandomFloat(0.0f, glm::two_pi<float>());
        const float r = glm::sqrt(glm::max(0.0f, 1.0f - z * z));
        return Vec3(glm::cos(angle) * r, z, glm::sin(angle) * r);
    }

    Vec2 RandomCircle()
    {
        const float angle = RandomFloat(0.0f, glm::two_pi<float>());
        return Vec2(glm::cos(angle), glm::sin(angle));
    }

    static Vec3 ToWorld(const Vec2& inPosition, float inHeight)
    {
        return Vec3(inPosition.x, inHeight, inPosition.y);
    }

    static float GetYaw(const Vec2& inDirection)
    {
        return glm::atan(-inDirection.x, -inDirection.y);
    }

    static float LerpAngle(float inFrom, float inTo, float inT)
    {
        const float delta = glm::mod(inTo - inFrom + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
        return inFrom + delta * inT;
    }

    static Quat LookRotation(const Vec3& inDirection)
    {
        const float length = glm::length(inDirection);

        if (length < 1e-5f)
            return Quat(1.0f, 0.0f, 0.0f, 0.0f);

        const Vec3 direction = inDirection / length;
        const Vec3 up = glm::abs(direction.y) > 0.98f ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);
        return glm::quatLookAtRH(direction, up);
    }

    static float EaseOutCubic(float inT)
    {
        const float t = 1.0f - glm::clamp(inT, 0.0f, 1.0f);
        return 1.0f - t * t * t;
    }

    static float EaseOutBack(float inT)
    {
        const float t = glm::clamp(inT, 0.0f, 1.0f) - 1.0f;
        return 1.0f + 2.7f * t * t * t + 1.7f * t * t;
    }

    static Vec2 ClampToArena(const Vec2& inPosition, float inRadius)
    {
        return glm::clamp(inPosition, Vec2(-cArenaHalfSize + inRadius), Vec2(cArenaHalfSize - inRadius));
    }

    static int GetRigPart(const String& inName)
    {
        static const HashMap<String, int> cParts =
        {
            { "Body", RIG_BODY }, { "Main", RIG_BODY }, { "Base", RIG_BODY }, { "Head", RIG_HEAD }, { "Lid", RIG_HEAD },
            { "ArmL", RIG_ARM_L }, { "ArmR", RIG_ARM_R }, { "LegL", RIG_LEG_L }, { "LegR", RIG_LEG_R }, { "WingL", RIG_WING_L }, { "WingR", RIG_WING_R },
        };

        const auto part = cParts.find(inName);
        return part != cParts.end() ? part->second : -1;
    }

    uint32_t LoadTexture(const char* inFile, bool inSRGB, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA)
    {
        if (!fs::exists(inFile))
            return 0;

        const String cached = TextureAsset::GetCachedPath(inFile);

        if (!fs::exists(cached) || fs::last_write_time(cached) < fs::last_write_time(inFile))
            TextureAsset::Convert(inFile);

        Assets* assets = m_App->GetAssets();
        TextureAsset::Ptr asset = assets ? assets->GetAsset<TextureAsset>(cached) : nullptr;

        return asset ? m_App->GetRenderInterface()->UploadTextureFromAsset(asset, inSRGB, inSwizzle) : 0;
    }

    void LoadTextures()
    {
        if (m_PaletteTexture == 0)
            m_PaletteTexture = LoadTexture(cPaletteFile, true);

        if (m_PaletteEmissiveTexture == 0)
            m_PaletteEmissiveTexture = LoadTexture(cPaletteEmissiveFile, false);

        for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
        {
            GroundTextures& textures = m_GroundTextures[layer];
            const String prefix = std::format("Assets/DuskSwarm/Ground/{}", cGroundLayers[layer].mTextures);

            if (textures.mAlbedo == 0)
                textures.mAlbedo = LoadTexture(( prefix + "Albedo.png" ).c_str(), true);

            if (textures.mNormal == 0)
                textures.mNormal = LoadTexture(( prefix + "Normal.png" ).c_str(), false, cNormalMapSwizzleAG);

            if (textures.mRoughness == 0)
                textures.mRoughness = LoadTexture(( prefix + "Roughness.png" ).c_str(), false, TEXTURE_SWIZZLE_GGGG);
        }

        const auto LoadUITexture = [this](uint32_t& outTexture, const char* inFile)
        {
            if (outTexture == 0)
                outTexture = LoadTexture(inFile, false);
        };

        LoadUITexture(m_PanelTexture, "Assets/DuskSwarm/UI/Panel.png");
        LoadUITexture(m_ParchmentTexture, "Assets/DuskSwarm/UI/Parchment.png");
        LoadUITexture(m_SlotTexture, "Assets/DuskSwarm/UI/Slot.png");
        LoadUITexture(m_ButtonTexture, "Assets/DuskSwarm/UI/Button.png");
        LoadUITexture(m_OrbFrameTexture, "Assets/DuskSwarm/UI/OrbFrame.png");
        LoadUITexture(m_OrbFillTexture, "Assets/DuskSwarm/UI/OrbFill.png");
        LoadUITexture(m_HitsplatTexture, "Assets/DuskSwarm/UI/Hitsplat.png");
        LoadUITexture(m_HitsplatMaxTexture, "Assets/DuskSwarm/UI/HitsplatMax.png");
        LoadUITexture(m_HintArrowTexture, "Assets/DuskSwarm/UI/HintArrow.png");
        LoadUITexture(m_HeartTexture, "Assets/DuskSwarm/UI/Heart.png");
        LoadUITexture(m_GemTexture, "Assets/DuskSwarm/UI/Gem.png");
        LoadUITexture(m_DashIconTexture, "Assets/DuskSwarm/UI/Icons/Dash.png");

        for (int upgrade = 0; upgrade <= UPGRADE_COUNT; upgrade++)
            LoadUITexture(m_UpgradeIconTextures[upgrade], std::format("Assets/DuskSwarm/UI/Icons/{}.png", cUpgradeIcons[upgrade]).c_str());

        m_HudFont = g_UIRenderer.AddFont(cHudFontFile);
        m_TitleFont = g_UIRenderer.AddFont(cTitleFontFile);
    }

    void ReleaseModels()
    {
        m_MeshTemplates.clear();
    }

    void LoadModels()
    {
        ReleaseModels();

        Scene scene(nullptr);
        GltfImporter importer(scene, nullptr);

        if (!importer.LoadFromFile(cModelFile, nullptr))
        {
            Log(std::format("Failed to load {}", cModelFile));
            return;
        }

        for (const auto& [entity, mesh] : scene.Each<Mesh>())
        {
            const String& name = scene.Get<Name>(entity).name;
            const size_t split = name.find('_');

            const String asset = name.substr(0, split);
            const String part = split != String::npos ? name.substr(split + 1) : String("Main");

            PartTemplate& part_template = m_MeshTemplates[asset].emplace_back();
            part_template.mName = part;
            part_template.mJoint = scene.Get<Transform>(entity).position;
            part_template.mMesh = mesh;
        }
    }

    Array<Model> CreateModels(const String& inName, int inCount, Entity inMaterial = Entity::Null)
    {
        Array<Model> models(inCount);
        const Entity material = inMaterial != Entity::Null ? inMaterial : m_PaletteMaterial;

        for (Model& model : models)
            model.mRoot = CreateNode();

        const auto templates = m_MeshTemplates.find(inName);

        if (templates == m_MeshTemplates.end())
        {
            Mesh cube;
            cube.SetAsset(MeshAsset::Find(*m_App->GetAssets(), MeshAsset::cBuiltinCube), 0);

            for (Model& model : models)
            {
                const Node fallback = CreateMeshNode(cube, material, model.mRoot);
                SetTransform(fallback, Vec3(0.0f, 0.5f, 0.0f), Vec3(0.6f));

                model.mParts[RIG_BODY] = RigPart { .mNode = fallback, .mRest = Vec3(0.0f, 0.5f, 0.0f) };
                model.mMeshes.push_back(ModelMesh { .mNode = fallback, .mMaterial = material });
            }

            return models;
        }

        for (const PartTemplate& part : templates->second)
        {
            const int role = GetRigPart(part.mName);

            for (Model& model : models)
            {
                const Node node = CreateMeshNode(part.mMesh, material, model.mRoot);
                SetTransform(node, part.mJoint, Vec3(1.0f));

                if (role >= 0 && model.mParts[role].mNode == Node::Null)
                    model.mParts[role] = RigPart { .mNode = node, .mRest = part.mJoint };

                model.mMeshes.push_back(ModelMesh { .mNode = node, .mMaterial = material });
            }
        }

        return models;
    }

    Model CreateModel(const String& inName, Entity inMaterial = Entity::Null)
    {
        return CreateModels(inName, 1, inMaterial)[0];
    }

    void CreateMaterials()
    {
        m_PaletteMaterial = CreateMaterial(Vec3(1.0f), Vec3(0.0f), 0.85f);

        if (Material* palette = FindComponent<Material>(m_PaletteMaterial))
        {
            palette->gpuAlbedoMap = m_PaletteTexture;
            palette->gpuEmissiveMap = m_PaletteEmissiveTexture;
            palette->emissive = m_PaletteEmissiveTexture != 0 ? Vec3(16000.0f) : Vec3(0.0f);
        }

        for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
        {
            m_GroundMaterials[layer] = CreateMaterial(cGroundLayers[layer].mTint, Vec3(0.0f), 1.0f);

            if (Material* ground = FindComponent<Material>(m_GroundMaterials[layer]))
            {
                ground->gpuAlbedoMap = m_GroundTextures[layer].mAlbedo;
                ground->gpuNormalMap = m_GroundTextures[layer].mNormal;
                ground->gpuRoughnessMap = m_GroundTextures[layer].mRoughness;

                if (layer == GROUND_GRASS)
                {
                    ground->samplerFilter = MATERIAL_SAMPLER_FILTER_POINT;
                    m_App->GetRenderInterface()->UploadMaterialSampler(*ground);
                }
            }
        }

        m_FlashMaterial = CreateMaterial(Vec3(1.0f), Vec3(1.0f, 0.9f, 0.85f) * 5000.0f, 0.3f);
        m_EliteFlashMaterial = CreateMaterial(Vec3(1.0f, 0.85f, 0.4f), Vec3(1.0f, 0.75f, 0.3f) * 5000.0f, 0.3f);
        m_BoltMaterial = CreateMaterial(Vec3(0.35f, 0.85f, 1.0f), Vec3(0.35f, 0.85f, 1.0f) * 12000.0f, 0.2f);
        m_GemMaterials[0] = CreateMaterial(Vec3(0.3f, 1.0f, 0.5f), Vec3(0.3f, 1.0f, 0.5f) * 6000.0f, 0.2f);
        m_GemMaterials[1] = CreateMaterial(Vec3(0.3f, 0.6f, 1.0f), Vec3(0.3f, 0.6f, 1.0f) * 8000.0f, 0.2f);
        m_GemMaterials[2] = CreateMaterial(Vec3(1.0f, 0.3f, 0.3f), Vec3(1.0f, 0.25f, 0.2f) * 10000.0f, 0.2f);
        m_DangerMaterial = CreateMaterial(Vec3(0.35f, 0.04f, 0.03f), Vec3(1.0f, 0.12f, 0.08f) * 900.0f, 0.6f);
        m_PoisonTelegraphMaterial = CreateMaterial(Vec3(0.15f, 0.3f, 0.05f), Vec3(0.45f, 1.0f, 0.2f) * 900.0f, 0.6f);

        const auto Emissive = [this](Vec3 inColor, float inStrength) { return CreateMaterial(inColor, inColor * inStrength, 0.3f); };

        m_FXMaterials[FX_FIRE]   = Emissive(Vec3(1.0f, 0.4f, 0.1f), 3500.0f);
        m_FXMaterials[FX_EMBER]  = Emissive(Vec3(1.0f, 0.62f, 0.25f), 7000.0f);
        m_FXMaterials[FX_SPARK]  = Emissive(Vec3(1.0f, 0.92f, 0.75f), 8000.0f);
        m_FXMaterials[FX_ARCANE] = Emissive(Vec3(0.3f, 0.8f, 1.0f), 5000.0f);
        m_FXMaterials[FX_PURPLE] = Emissive(Vec3(0.7f, 0.3f, 1.0f), 4500.0f);
        m_FXMaterials[FX_POISON] = Emissive(Vec3(0.5f, 1.0f, 0.25f), 3500.0f);
        m_FXMaterials[FX_GOLD]   = Emissive(Vec3(1.0f, 0.75f, 0.25f), 5000.0f);
        m_FXMaterials[FX_DANGER] = Emissive(Vec3(1.0f, 0.15f, 0.1f), 4000.0f);
        m_FXMaterials[FX_HEAL]   = Emissive(Vec3(0.4f, 1.0f, 0.5f), 4000.0f);
        m_FXMaterials[FX_WHITE]  = Emissive(Vec3(0.9f, 0.95f, 1.0f), 9000.0f);
        m_FXMaterials[FX_SMOKE]  = CreateMaterial(Vec3(0.07f, 0.07f, 0.08f), Vec3(0.0f), 1.0f);
        m_FXMaterials[FX_DIRT]   = CreateMaterial(Vec3(0.25f, 0.18f, 0.12f), Vec3(0.0f), 1.0f);
        m_FXMaterials[FX_BONE]   = CreateMaterial(Vec3(0.8f, 0.76f, 0.66f), Vec3(0.0f), 0.8f);
        m_FXMaterials[FX_GOO]    = CreateMaterial(Vec3(0.35f, 0.45f, 0.15f), Vec3(0.2f, 0.4f, 0.05f) * 800.0f, 0.4f);
        m_FXMaterials[FX_FLESH]  = CreateMaterial(Vec3(0.3f, 0.05f, 0.05f), Vec3(0.0f), 0.6f);
    }

    void CreateArena()
    {
        const Model ground = CreateModel("Ground");
        SetTransform(ground.mRoot, Vec3(0.0f), Vec3(1.0f));

        const auto ground_parts = m_MeshTemplates.find("Ground");

        for (size_t index = 0; ground_parts != m_MeshTemplates.end() && index < ground.mMeshes.size(); index++)
        {
            const String& part = ground_parts->second[index].mName;

            for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
            {
                if (part == cGroundLayers[layer].mPart && m_GroundTextures[layer].mAlbedo != 0)
                    SetMaterial(ground.mMeshes[index].mNode, m_GroundMaterials[layer]);
            }
        }

        const float wall_offset = cArenaHalfSize + 0.5f;

        Array<Model> walls = CreateModels("WallSegment", 80);

        for (int index = 0; index < 80; index++)
        {
            const int side = index / 20;
            const float along = -cArenaHalfSize + 2.0f + float(index % 20) * 4.0f;

            Vec3 position = Vec3(along, 0.0f, side == 0 ? -wall_offset : wall_offset);
            float yaw = 0.0f;

            if (side >= 2)
            {
                position = Vec3(side == 2 ? -wall_offset : wall_offset, 0.0f, along);
                yaw = glm::half_pi<float>();
            }

            SetTransform(walls[index].mRoot, position, Vec3(1.0f, RandomFloat(0.9f, 1.15f), 1.0f), Quat(Vec3(0.0f, yaw + ( index % 2 ? glm::pi<float>() : 0.0f ), 0.0f)));
        }

        Array<Vec2> lantern_positions;

        for (float offset = -cArenaHalfSize + 5.0f; offset <= cArenaHalfSize - 5.0f; offset += 10.0f)
        {
            for (const Vec2& position : { Vec2(offset, -wall_offset + 1.0f), Vec2(offset, wall_offset - 1.0f), Vec2(-wall_offset + 1.0f, offset), Vec2(wall_offset - 1.0f, offset) })
                lantern_positions.push_back(position);
        }

        Array<Model> lanterns = CreateModels("LanternPost", int(lantern_positions.size()));

        for (size_t index = 0; index < lanterns.size(); index++)
            SetTransform(lanterns[index].mRoot, ToWorld(lantern_positions[index], 0.0f), Vec3(1.0f), Quat(Vec3(0.0f, RandomFloat(-0.2f, 0.2f), 0.0f)));

        m_Obstacles.clear();

        const auto PlaceObstacle = [this](float inRadius, Vec2& outPosition)
        {
            for (int attempt = 0; attempt < 32; attempt++)
            {
                const Vec2 position = Vec2(RandomFloat(-cArenaHalfSize + 3.0f, cArenaHalfSize - 3.0f), RandomFloat(-cArenaHalfSize + 3.0f, cArenaHalfSize - 3.0f));

                if (glm::length(position) < 7.0f)
                    continue;

                bool overlaps = false;

                for (const Obstacle& obstacle : m_Obstacles)
                    overlaps |= glm::distance(obstacle.mPosition, position) < obstacle.mRadius + inRadius + 1.5f;

                if (overlaps)
                    continue;

                outPosition = position;
                m_Obstacles.push_back(Obstacle { .mPosition = position, .mRadius = inRadius });
                return true;
            }

            return false;
        };

        const auto PlaceGroup = [&](const char* inModel, int inCount, float inRadius, float inScaleMin, float inScaleMax, float inTilt)
        {
            Array<Vec2> positions;
            Vec2 position;

            for (int index = 0; index < inCount; index++)
            {
                if (PlaceObstacle(inRadius, position))
                    positions.push_back(position);
            }

            Array<Model> models = CreateModels(inModel, int(positions.size()));

            for (size_t index = 0; index < models.size(); index++)
            {
                const float scale = RandomFloat(inScaleMin, inScaleMax);
                const Quat rotation = Quat(Vec3(RandomFloat(-inTilt, inTilt), RandomFloat(0.0f, glm::two_pi<float>()), RandomFloat(-inTilt, inTilt)));
                SetTransform(models[index].mRoot, ToWorld(positions[index], 0.0f), Vec3(scale), rotation);
            }
        };

        PlaceGroup("Gravestone", 20, 0.45f, 0.9f, 1.1f, 0.08f);
        PlaceGroup("GraveCross", 12, 0.4f, 0.9f, 1.15f, 0.08f);
        PlaceGroup("Rock0", 6, 0.75f, 0.85f, 1.15f, 0.0f);
        PlaceGroup("Rock1", 6, 0.75f, 0.85f, 1.15f, 0.0f);
        PlaceGroup("Rock2", 6, 0.75f, 0.85f, 1.15f, 0.0f);
        PlaceGroup("DeadTree0", 7, 0.4f, 0.85f, 1.25f, 0.04f);
        PlaceGroup("DeadTree1", 7, 0.4f, 0.85f, 1.25f, 0.04f);
        PlaceGroup("Pillar", 8, 0.5f, 0.9f, 1.15f, 0.03f);

        const auto Scatter = [&](const char* inModel, int inCount, float inScaleMin, float inScaleMax)
        {
            Array<Model> models = CreateModels(inModel, inCount);

            for (Model& model : models)
            {
                const Vec2 position = Vec2(RandomFloat(-cArenaHalfSize + 1.0f, cArenaHalfSize - 1.0f), RandomFloat(-cArenaHalfSize + 1.0f, cArenaHalfSize - 1.0f));
                SetTransform(model.mRoot, ToWorld(position, 0.0f), Vec3(RandomFloat(inScaleMin, inScaleMax)), Quat(Vec3(0.0f, RandomFloat(0.0f, glm::two_pi<float>()), 0.0f)));
            }
        };

        Scatter("GrassTuft", 180, 0.8f, 1.4f);
        Scatter("Mushrooms", 30, 0.8f, 1.3f);
        Scatter("Bones", 18, 0.9f, 1.2f);
    }

    void CreatePlayer()
    {
        m_PlayerEntity = CreateEntity("Player");
        m_PlayerModel = CreateModel("Hero");

        const Entity torch = CreateEntity("Torch", m_PlayerEntity);
        SetTransform(torch, Vec3(0.0f, 3.0f, 0.0f), Vec3(1.0f));

        Light& light = m_Scene->Add<Light>(torch);
        light.type = LIGHT_TYPE_POINT;
        light.color = Vec4(1.0f, 0.72f, 0.45f, 400000.0f);
        light.attributes.x = 12.0f;
    }

    void CreatePools()
    {
        m_Enemies.clear();

        for (int type = 0; type < ENEMY_TYPE_COUNT; type++)
        {
            const EnemyArchetype& archetype = cEnemyArchetypes[type];
            Array<Model> models = CreateModels(archetype.mName, archetype.mPoolSize);

            for (Model& model : models)
            {
                Enemy& enemy = m_Enemies.emplace_back();
                enemy.mType = type;
                enemy.mModel = model;
                Park(model.mRoot);
            }
        }

        m_Projectiles.resize(cProjectilePoolSize);
        Array<Model> bolts = CreateModels("Bolt", cProjectilePoolSize, m_BoltMaterial);

        for (int index = 0; index < cProjectilePoolSize; index++)
        {
            m_Projectiles[index].mNode = bolts[index].mRoot;
            Park(bolts[index].mRoot);
        }

        m_Fireballs.resize(cFireballPoolSize);
        Array<Model> fireballs = CreateModels("Fireball", cFireballPoolSize);

        for (int index = 0; index < cFireballPoolSize; index++)
        {
            m_Fireballs[index].mNode = fireballs[index].mRoot;
            Park(fireballs[index].mRoot);
        }

        m_Arrows.resize(cArrowPoolSize);
        Array<Model> arrows = CreateModels("Arrow", cArrowPoolSize);

        for (int index = 0; index < cArrowPoolSize; index++)
        {
            m_Arrows[index].mNode = arrows[index].mRoot;
            m_Arrows[index].mRadius = 0.25f;
            Park(arrows[index].mRoot);
        }

        m_Orbs.resize(cOrbPoolSize);
        Array<Model> orbs = CreateModels("Orb", cOrbPoolSize);

        for (int index = 0; index < cOrbPoolSize; index++)
        {
            m_Orbs[index].mNode = orbs[index].mRoot;
            m_Orbs[index].mRadius = 0.35f;
            Park(orbs[index].mRoot);
        }

        m_Meteors.resize(cMeteorPoolSize);
        Array<Model> meteors = CreateModels("Meteor", cMeteorPoolSize);

        for (int index = 0; index < cMeteorPoolSize; index++)
        {
            m_Meteors[index].mNode = meteors[index].mRoot;
            Park(meteors[index].mRoot);
        }

        m_Gems.resize(cGemPoolSize);
        Array<Model> gems = CreateModels("Gem", cGemPoolSize, m_GemMaterials[0]);

        for (int index = 0; index < cGemPoolSize; index++)
        {
            m_Gems[index].mModel = gems[index];
            Park(gems[index].mRoot);
        }

        Array<Model> blades = CreateModels("Blade", cMaxBlades);

        for (int index = 0; index < cMaxBlades; index++)
        {
            m_Blades[index] = blades[index].mRoot;
            Park(m_Blades[index]);
        }

        m_Pickups.clear();

        for (int type = 0; type < PICKUP_COUNT; type++)
        {
            Array<Model> models = CreateModels(cPickupModels[type], cPickupsPerType);

            for (Model& model : models)
            {
                Pickup& pickup = m_Pickups.emplace_back();
                pickup.mType = type;
                pickup.mModel = model;
                Park(model.mRoot);
            }
        }

        Array<Model> pickup_glows = CreateModels("Ring", int(m_Pickups.size()), m_FXMaterials[FX_GOLD]);

        for (size_t index = 0; index < m_Pickups.size(); index++)
        {
            m_Pickups[index].mGlow = pickup_glows[index].mRoot;
            Park(pickup_glows[index].mRoot);
        }

        for (int mesh = 0; mesh < PARTICLE_MESH_COUNT; mesh++)
        {
            m_Particles[mesh].clear();
            m_ParticleCursor[mesh] = 0;

            Array<Model> models = CreateModels(cParticleModels[mesh], cParticlePoolSizes[mesh], m_FXMaterials[FX_SPARK]);

            for (Model& model : models)
            {
                Particle& particle = m_Particles[mesh].emplace_back();
                particle.mNode = model.mRoot;
                particle.mMaterial = m_FXMaterials[FX_SPARK];
                Park(model.mRoot);
            }
        }

        m_NovaBursts.resize(cNovaBurstPoolSize);

        const StaticArray<std::pair<const char*, Vec4>, 3> nova_layers =
        {
            std::pair { "Starburst", Vec4(0.6f, 0.22f, 1.0f, 900.0f) },
            std::pair { "Starburst", Vec4(0.85f, 0.6f, 1.0f, 1400.0f) },
            std::pair { "Disc", Vec4(1.0f, 0.95f, 1.0f, 3000.0f) },
        };

        for (NovaBurst& burst : m_NovaBursts)
        {
            for (size_t layer = 0; layer < nova_layers.size(); layer++)
            {
                const Vec3 color = Vec3(nova_layers[layer].second);
                burst.mMaterials[layer] = CreateMaterial(color, color * nova_layers[layer].second.w, 0.5f);

                if (Material* material = FindComponent<Material>(burst.mMaterials[layer]))
                    material->blendMode = MATERIAL_BLEND_MODE_BLENDED;

                burst.mNodes[layer] = CreateModels(nova_layers[layer].first, 1, burst.mMaterials[layer])[0].mRoot;
                Park(burst.mNodes[layer]);
            }
        }

        m_Telegraphs.resize(cTelegraphPoolSize);
        Array<Model> discs = CreateModels("Disc", cTelegraphPoolSize, m_DangerMaterial);
        Array<Model> telegraph_rings = CreateModels("Ring", cTelegraphPoolSize, m_DangerMaterial);

        for (int index = 0; index < cTelegraphPoolSize; index++)
        {
            m_Telegraphs[index].mDisc = discs[index].mRoot;
            m_Telegraphs[index].mRing = telegraph_rings[index].mRoot;
            Park(discs[index].mRoot);
            Park(telegraph_rings[index].mRoot);
        }

        m_Beams.resize(cBeamPoolSize);
        Array<Model> beams = CreateModels("Beam", cBeamPoolSize, m_FXMaterials[FX_WHITE]);

        for (int index = 0; index < cBeamPoolSize; index++)
        {
            m_Beams[index].mNode = beams[index].mRoot;
            Park(beams[index].mRoot);
        }

        m_EliteAuras.resize(cEliteAuraPoolSize);
        Array<Model> auras = CreateModels("Ring", cEliteAuraPoolSize, m_FXMaterials[FX_GOLD]);

        for (int index = 0; index < cEliteAuraPoolSize; index++)
        {
            m_EliteAuras[index].mNode = auras[index].mRoot;
            Park(auras[index].mRoot);
        }

        m_Lights.resize(cLightPoolSize);

        for (FlashLight& flash : m_Lights)
        {
            flash.mEntity = CreateEntity("Flash Light");
            SetTransform(flash.mEntity, cParkedPosition, Vec3(1.0f));

            Light& light = m_Scene->Add<Light>(flash.mEntity);
            light.type = LIGHT_TYPE_POINT;
            light.color = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
            light.attributes.x = 10.0f;
        }
    }

    void CreateCamera()
    {
        m_CameraEntity = CreateEntity("Swarm Camera");

        Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
        camera.SetFov(50.0f);

        m_App->SetCameraEntity(m_CameraEntity);
    }

    Entity CreateMusic(StringView inName, const char* inFile, float inVolume)
    {
        const Entity entity = CreateEntity(inName);

        SoundSource& music = m_Scene->Add<SoundSource>(entity);
        music.file = inFile;
        music.category = SOUND_CATEGORY_MUSIC;
        music.volume = inVolume;
        music.loop = true;
        music.stream = true;
        music.spatial = false;
        music.fadeInTime = 2.0f;
        music.fadeOutTime = 1.5f;

        return entity;
    }

    void CreateAudio()
    {
        m_ListenerEntity = CreateEntity("Listener");
        m_Scene->Add<AudioListener>(m_ListenerEntity);

        m_MusicEntity = CreateMusic("Music", cMusicSound, 0.5f);
        m_BossMusicEntity = CreateMusic("Boss Music", cBossMusicSound, 0.55f);

        const Entity ambience_entity = CreateEntity("Night Ambience");

        SoundSource& ambience = m_Scene->Add<SoundSource>(ambience_entity);
        ambience.file = cAmbienceSound;
        ambience.category = SOUND_CATEGORY_AMBIENCE;
        ambience.volume = 0.7f;
        ambience.loop = true;
        ambience.stream = true;
        ambience.spatial = false;
        ambience.playOnStart = true;
        ambience.fadeInTime = 2.0f;

        if (Audio* audio = GetAudio())
        {
            for (const char* sound : cSoundEffects)
                audio->Preload(sound);
        }
    }

    bool CanPlaySfx(const char* inFile, float inMinInterval)
    {
        if (inMinInterval <= 0.0f)
            return true;

        const auto [entry, inserted] = m_SoundTimes.try_emplace(inFile, m_Time);

        if (!inserted)
        {
            if (m_Time - entry->second < inMinInterval)
                return false;

            entry->second = m_Time;
        }

        return true;
    }

    void PlaySfx(const char* inFile, float inVolume = 1.0f, float inPitch = 1.0f, float inMinInterval = 0.0f)
    {
        Audio* audio = GetAudio();

        if (audio && CanPlaySfx(inFile, inMinInterval))
            audio->PlayOneShot(inFile, inVolume, inPitch);
    }

    void PlaySfxAt(const char* inFile, const Vec3& inPosition, float inVolume = 1.0f, float inPitch = 1.0f, float inMinInterval = 0.0f)
    {
        Audio* audio = GetAudio();

        if (audio && CanPlaySfx(inFile, inMinInterval))
            audio->PlayOneShot(inFile, inPosition, inVolume, inPitch, cSoundMinDistance);
    }

    void PlayEntitySound(Entity inEntity)
    {
        if (Audio* audio = GetAudio())
            audio->Play(*m_Scene, inEntity);
    }

    void StopEntitySound(Entity inEntity)
    {
        if (Audio* audio = GetAudio())
            audio->Stop(*m_Scene, inEntity);
    }

    void PlayMusic(bool inBoss)
    {
        StopEntitySound(inBoss ? m_MusicEntity : m_BossMusicEntity);
        PlayEntitySound(inBoss ? m_BossMusicEntity : m_MusicEntity);
    }

    void StopMusic()
    {
        StopEntitySound(m_MusicEntity);
        StopEntitySound(m_BossMusicEntity);
    }

    void UpdateSounds()
    {
        PROFILE_FUNCTION_CPU();

        SetTransform(m_ListenerEntity, ToWorld(m_PlayerPosition, cListenerHeight), Vec3(1.0f));

        if (m_State == STATE_GAME_OVER && !m_EndJinglePlayed && m_PlayerDeathTimer >= cGameOverJingleDelay)
        {
            m_EndJinglePlayed = true;
            PlaySfx(cGameOverSound, 0.8f);
        }
    }

    void DestroyRuntimeEntities()
    {
        Array<Entity> roots;

        for (const auto& [entity, name] : m_Scene->Each<Name>())
        {
            if (name.name == cRuntimeRootName && m_Scene->GetParent(entity) == m_Scene->GetRootEntity())
                roots.push_back(entity);
        }

        if (!roots.empty())
            m_App->SetCameraEntity(Entity::Null);

        for (Entity root : roots)
            m_Scene->Destroy(root);

        m_RootEntity = Entity::Null;
        m_CameraEntity = Entity::Null;
        m_PlayerEntity = Entity::Null;
        m_ListenerEntity = Entity::Null;
        m_MusicEntity = Entity::Null;
        m_BossMusicEntity = Entity::Null;
        m_PlayerModel = Model();
        m_Blades.fill(Node::Null);
        m_Nodes.clear();
        m_MeshBatches.clear();
        m_MeshBatchComponents.clear();
        m_Enemies.clear();
        m_Projectiles.clear();
        m_Fireballs.clear();
        m_Arrows.clear();
        m_Orbs.clear();
        m_Meteors.clear();
        m_Gems.clear();
        m_Pickups.clear();
        m_NovaBursts.clear();
        m_Telegraphs.clear();
        m_Beams.clear();
        m_Lights.clear();
        m_EliteAuras.clear();
        m_Obstacles.clear();

        for (Array<Particle>& particles : m_Particles)
            particles.clear();

        ReleaseModels();
    }

    void NewGame()
    {
        for (Enemy& enemy : m_Enemies)
        {
            enemy.mActive = false;
            enemy.mDying = false;
            enemy.mAura = -1;
            enemy.mTelegraph = -1;
            RestoreModelMaterials(enemy.mModel);
            Park(enemy.mModel.mRoot);
        }

        for (Projectile& projectile : m_Projectiles)
        {
            projectile.mActive = false;
            Park(projectile.mNode);
        }

        for (Fireball& fireball : m_Fireballs)
        {
            fireball.mActive = false;
            Park(fireball.mNode);
        }

        for (Array<EnemyProjectile>* pool : { &m_Arrows, &m_Orbs })
        {
            for (EnemyProjectile& projectile : *pool)
            {
                projectile.mActive = false;
                Park(projectile.mNode);
            }
        }

        for (Meteor& meteor : m_Meteors)
        {
            meteor.mActive = false;
            Park(meteor.mNode);
        }

        for (Gem& gem : m_Gems)
        {
            gem.mActive = false;
            Park(gem.mModel.mRoot);
        }

        for (Pickup& pickup : m_Pickups)
        {
            pickup.mActive = false;
            Park(pickup.mModel.mRoot);
            Park(pickup.mGlow);
        }

        for (Array<Particle>& particles : m_Particles)
        {
            for (Particle& particle : particles)
            {
                particle.mActive = false;
                Park(particle.mNode);
            }
        }

        for (NovaBurst& burst : m_NovaBursts)
        {
            burst.mActive = false;

            for (Node node : burst.mNodes)
                Park(node);
        }

        for (Telegraph& telegraph : m_Telegraphs)
            ReleaseTelegraph(telegraph);

        for (Beam& beam : m_Beams)
        {
            beam.mActive = false;
            Park(beam.mNode);
        }

        for (EliteAura& aura : m_EliteAuras)
        {
            aura.mActive = false;
            Park(aura.mNode);
        }

        for (FlashLight& flash : m_Lights)
        {
            flash.mActive = false;

            if (Light* light = FindComponent<Light>(flash.mEntity))
                light->color.a = 0.0f;
        }

        for (int type = 0; type < ENEMY_TYPE_COUNT; type++)
        {
            m_FreeEnemies[type].clear();

            for (int index = int(m_Enemies.size()) - 1; index >= 0; index--)
            {
                if (m_Enemies[index].mType == type)
                    m_FreeEnemies[type].push_back(index);
            }
        }

        for (Node blade : m_Blades)
            Park(blade);

        m_UpgradeLevels.fill(0);
        m_UpgradeLevels[UPGRADE_BOLT] = 1;

        m_State = STATE_PLAYING;
        m_RunTime = 0.0f;
        m_PlayerPosition = Vec2(0.0f);
        m_PlayerVelocity = Vec2(0.0f);
        m_PlayerYaw = 0.0f;
        m_PlayerWalkPhase = 0.0f;
        m_PlayerDeathTimer = 0.0f;
        m_Health = cPlayerBaseHealth;
        m_InvulnerableTimer = 0.0f;
        m_DashTimer = -1.0f;
        m_DashCooldownTimer = 0.0f;
        m_CastTimer = 0.0f;
        m_HitFlash = 0.0f;
        m_ScreenFlash = 0.0f;
        m_Trauma = 0.0f;
        m_HitStop = 0.0f;
        m_CameraKick = 0.0f;
        m_Level = 1;
        m_Experience = 0;
        m_PendingLevelUps = 0;
        m_Kills = 0;
        m_EliteKills = 0;
        m_BossKills = 0;
        m_Streak = 0;
        m_BestStreak = 0;
        m_StreakTimer = 0.0f;
        m_DamageDealt = 0.0f;
        m_SpawnAccumulator = 0.0f;
        m_NextSwarmTime = cSwarmInterval;
        m_NextBruteTime = cBruteInterval;
        m_NextBoss = 0;
        m_BossEnemy = -1;
        m_BossPattern = 0;
        m_BoltTimer = 0.5f;
        m_NovaTimer = 2.0f;
        m_FireballTimer = 1.0f;
        m_LightningTimer = 1.0f;
        m_MeteorTimer = 2.0f;
        m_BladeAngle = 0.0f;
        m_EndTime = 0.0f;
        m_ChoiceCount = 0;
        m_DamageNumbers.clear();
        m_Popups.clear();
        m_XPDrops.clear();
        m_LichKills = 0;
        m_ChatMessages.clear();

        AddChat("Welcome to Dusk Swarm.", cChatBlack);
        AddChat("WASD to move, Space to dash. Your weapons fire on their own.", cChatBlue);
        AddChat("Survive until dawn.", cChatRed);
        m_CameraTarget = Vec3(0.0f);
        m_FacingDirection = Vec2(0.0f, -1.0f);
        m_GemStreak = 0;
        m_LastGemTime = -cGemStreakTime;
        m_EndJinglePlayed = false;

        PlayMusic(false);

        if (*m_StartWeapons > 0)
        {
            for (int upgrade = UPGRADE_BOLT; upgrade <= UPGRADE_METEOR; upgrade++)
                m_UpgradeLevels[upgrade] = glm::clamp(*m_StartWeapons, 1, cMaxUpgradeLevel);
        }

        if (*m_StartTime > 0.0f)
        {
            m_RunTime = *m_StartTime;

            while (m_NextSwarmTime <= m_RunTime)
                m_NextSwarmTime += cSwarmInterval;

            while (m_NextBruteTime <= m_RunTime)
                m_NextBruteTime += cBruteInterval;

            while (m_NextBoss < int(cBossTimes.size()) && cBossTimes[m_NextBoss] < m_RunTime)
                m_NextBoss++;
        }
    }

    float GetMaxHealth() const { return cPlayerBaseHealth + 25.0f * m_UpgradeLevels[UPGRADE_VITALITY]; }
    float GetMoveSpeed() const { return cPlayerBaseSpeed * ( 1.0f + 0.12f * m_UpgradeLevels[UPGRADE_BOOTS] ); }
    float GetMagnetRadius() const { return cBaseMagnetRadius * ( 1.0f + 0.4f * m_UpgradeLevels[UPGRADE_MAGNET] ); }
    float GetDamageMultiplier() const { return 1.0f + 0.15f * m_UpgradeLevels[UPGRADE_POWER]; }
    float GetCritChance() const { return 0.06f + 0.03f * m_UpgradeLevels[UPGRADE_POWER]; }
    float GetCooldownMultiplier() const { return glm::pow(0.92f, float(m_UpgradeLevels[UPGRADE_HASTE])); }
    float GetAreaMultiplier() const { return 1.0f + 0.12f * m_UpgradeLevels[UPGRADE_AREA]; }
    int   GetExperienceToNextLevel() const { return 5 + ( m_Level - 1 ) * 4 + ( m_Level - 1 ) * ( m_Level - 1 ) / 3; }
    Vec3  GetPlayerCenter() const { return ToWorld(m_PlayerPosition, 0.9f); }
    Vec3  GetStaffTip() const { return ToWorld(m_PlayerPosition, 1.75f) + Vec3(glm::cos(m_PlayerYaw) * 0.38f, 0.0f, -glm::sin(m_PlayerYaw) * 0.38f); }

    Vec2 GetInputDirection() const
    {
        Vec2 input = Vec2(0.0f);

        if (m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::UP))    input.y -= 1.0f;
        if (m_Input->IsKeyDown(Key::S) || m_Input->IsKeyDown(Key::DOWN))  input.y += 1.0f;
        if (m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::LEFT))  input.x -= 1.0f;
        if (m_Input->IsKeyDown(Key::D) || m_Input->IsKeyDown(Key::RIGHT)) input.x += 1.0f;

        return glm::length(input) > 0.0f ? glm::normalize(input) : input;
    }

    void TryDash()
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

    void UpdatePlayer(float inDeltaTime)
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

        m_PlayerPosition += m_PlayerVelocity * inDeltaTime;
        m_PlayerPosition = ResolveObstacles(m_PlayerPosition, cPlayerRadius);
        m_PlayerPosition = ClampToArena(m_PlayerPosition, cPlayerRadius);

        if (glm::length(input) > 0.0f)
            m_FacingDirection = input;

        const float max_health = GetMaxHealth();
        m_Health = glm::min(max_health, m_Health + 0.6f * m_UpgradeLevels[UPGRADE_REGEN] * inDeltaTime);

        m_InvulnerableTimer = glm::max(0.0f, m_InvulnerableTimer - inDeltaTime);
        m_CastTimer = glm::max(0.0f, m_CastTimer - inDeltaTime);

        m_StreakTimer -= inDeltaTime;

        if (m_StreakTimer <= 0.0f)
            m_Streak = 0;
    }

    Vec2 ResolveObstacles(Vec2 inPosition, float inRadius) const
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

    void DamagePlayer(float inDamage)
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

    void AddTrauma(float inAmount, float inCap = 1.0f)
    {
        if (m_Trauma < inCap)
            m_Trauma = glm::min(inCap, m_Trauma + inAmount);
    }

    void AddTraumaAt(const Vec3& inPosition, float inAmount)
    {
        const float distance = glm::distance(Vec2(inPosition.x, inPosition.z), m_PlayerPosition);
        AddTrauma(inAmount * glm::clamp(1.0f - distance / 30.0f, 0.15f, 1.0f), cAmbientTraumaCap);
    }

    void UpdateSpawning(float inDeltaTime)
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

    Vec2 GetSpawnPosition()
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

    int SpawnEnemy(int inType, const Vec2& inPosition, bool inElite = false)
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

    void BuildGrid()
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

    static IVec2 GetGridCoord(const Vec2& inPosition)
    {
        const Vec2 local = ( inPosition + Vec2(cArenaHalfSize + 2.0f) ) / cGridCellSize;
        return glm::clamp(IVec2(glm::floor(local)), IVec2(0), IVec2(cGridDim - 1));
    }

    static int GetGridCell(const Vec2& inPosition)
    {
        const IVec2 coord = GetGridCoord(inPosition);
        return coord.x + coord.y * cGridDim;
    }

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

    int FindNearestEnemy(const Vec2& inPosition, float inMaxDistance, int inExclude = -1, const Array<int>* inIgnore = nullptr)
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

    void UpdateEnemies(float inDeltaTime)
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

    void UpdateLich(Enemy& ioEnemy, float inDistance, const Vec2& inToPlayer, const Vec2& inSide, Vec2& outDesired, float inDeltaTime)
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

    void BruteSlam(Enemy& ioEnemy)
    {
        const float radius = 3.0f * ( ioEnemy.mElite ? 1.3f : 1.0f );
        const Vec2 center = ioEnemy.mPosition + ioEnemy.mDashDirection * 1.4f;

        ReleaseTelegraph(ioEnemy.mTelegraph);
        ioEnemy.mTelegraph = -1;

        if (glm::distance(center, m_PlayerPosition) < radius + cPlayerRadius)
            DamagePlayer(22.0f * ( ioEnemy.mElite ? 1.5f : 1.0f ));

        Explode(ToWorld(center, 0.1f), radius, 0.0f, EXPLOSION_DUST, false, 0.0f);
    }

    void BloaterBurst(int inIndex)
    {
        Enemy& enemy = m_Enemies[inIndex];

        ReleaseTelegraph(enemy.mTelegraph);
        enemy.mTelegraph = -1;

        Explode(ToWorld(enemy.mPosition, 0.6f), 2.8f, 25.0f, EXPLOSION_POISON, true, 18.0f * ( enemy.mElite ? 1.5f : 1.0f ));
        ReleaseAura(enemy);
        Despawn(inIndex);
    }

    void Despawn(int inIndex)
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

    void ReleaseAura(Enemy& ioEnemy)
    {
        if (ioEnemy.mAura >= 0)
        {
            m_EliteAuras[ioEnemy.mAura].mActive = false;
            Park(m_EliteAuras[ioEnemy.mAura].mNode);
            ioEnemy.mAura = -1;
        }
    }

    bool DamageEnemy(int inIndex, float inDamage, const Vec2& inKnockback, bool inCanCrit = true)
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

    void KillEnemy(int inIndex)
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

    void UpdateDying(float inDeltaTime)
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

    void FireEnemyProjectile(Array<EnemyProjectile>& ioPool, const Vec3& inPosition, const Vec3& inVelocity, float inDamage, float inLife)
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

    void UpdateEnemyProjectiles(float inDeltaTime)
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

    void UpdateWeapons(float inDeltaTime)
    {
        PROFILE_FUNCTION_CPU();

        const float damage_multiplier = GetDamageMultiplier();
        const float cooldown_multiplier = GetCooldownMultiplier();
        const float area = GetAreaMultiplier();

        if (const int level = m_UpgradeLevels[UPGRADE_BOLT]; level > 0)
        {
            m_BoltTimer -= inDeltaTime;

            if (m_BoltTimer <= 0.0f)
            {
                m_BoltTimer = ( 0.95f - 0.1f * ( level - 1 ) ) * cooldown_multiplier;
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

    void FireBolts(int inLevel, float inDamageMultiplier)
    {
        const int target = FindNearestEnemy(m_PlayerPosition, 18.0f);
        const Vec2 aim = target >= 0 ? glm::normalize(m_Enemies[target].mPosition - m_PlayerPosition + Vec2(0.0001f)) : m_FacingDirection;

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
                projectile.mPosition = m_PlayerPosition + direction * 0.6f;
                projectile.mVelocity = direction * 17.0f;
                projectile.mLifetime = 1.4f;
                projectile.mDamage = ( 3.0f + 1.0f * ( inLevel - 1 ) ) * inDamageMultiplier;
                projectile.mPierce = 1 + inLevel / 2;
                projectile.mLastHit = -1;
                break;
            }
        }

        m_CastTimer = 0.25f;
        PlaySfx(cBoltSound, 0.35f, RandomFloat(0.94f, 1.08f), 0.05f);
        Emit(GetStaffTip(), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 4, .mSpeedMin = 1.0f, .mSpeedMax = 3.0f, .mLifeMin = 0.15f, .mLifeMax = 0.25f, .mSizeMin = 0.1f, .mSizeMax = 0.18f, .mDrag = 3.0f });
    }

    void UpdateProjectiles(float inDeltaTime)
    {
        PROFILE_FUNCTION_CPU();

        for (Projectile& projectile : m_Projectiles)
        {
            if (!projectile.mActive)
                continue;

            const int target = FindNearestEnemy(projectile.mPosition, 6.0f, projectile.mLastHit);

            if (target >= 0)
            {
                const Vec2 desired = glm::normalize(m_Enemies[target].mPosition - projectile.mPosition + Vec2(0.0001f)) * 17.0f;
                projectile.mVelocity = glm::normalize(glm::mix(projectile.mVelocity, desired, 1.0f - glm::exp(-inDeltaTime * 4.0f))) * 17.0f;
            }

            projectile.mPosition += projectile.mVelocity * inDeltaTime;
            projectile.mLifetime -= inDeltaTime;

            if (RandomFloat(0.0f, 1.0f) < 0.5f)
                Emit(ToWorld(projectile.mPosition, 0.9f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 1, .mSpeedMin = 0.2f, .mSpeedMax = 0.8f, .mLifeMin = 0.12f, .mLifeMax = 0.22f, .mSizeMin = 0.1f, .mSizeMax = 0.15f, .mDrag = 2.0f });

            ForEachEnemyNear(projectile.mPosition, 0.2f, [&](int inIndex, Enemy& inEnemy)
            {
                if (projectile.mPierce <= 0 || inIndex == projectile.mLastHit)
                    return;

                projectile.mLastHit = inIndex;
                projectile.mPierce--;

                DamageEnemy(inIndex, projectile.mDamage, glm::normalize(projectile.mVelocity) * 4.0f);
                PlaySfxAt(cBoltHitSound, ToWorld(projectile.mPosition, 0.9f), 0.4f, RandomFloat(0.9f, 1.1f), 0.04f);
                Emit(ToWorld(projectile.mPosition, 0.9f), ParticleDesc { .mMesh = PARTICLE_SPARK, .mMaterial = FX_ARCANE, .mCount = 6, .mSpeedMin = 2.0f, .mSpeedMax = 5.0f, .mLifeMin = 0.15f, .mLifeMax = 0.3f, .mSizeMin = 0.15f, .mSizeMax = 0.25f, .mDrag = 3.0f });
            });

            const bool outside = glm::any(glm::greaterThan(glm::abs(projectile.mPosition), Vec2(cArenaHalfSize)));

            if (projectile.mPierce <= 0 || projectile.mLifetime <= 0.0f || outside)
            {
                projectile.mActive = false;
                Park(projectile.mNode);
            }
        }
    }

    void LaunchFireball(float inDamage, float inRadius)
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

    void UpdateFireballs(float inDeltaTime)
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

    void CastLightning(int inJumps, float inDamage)
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

    void SpawnLightningArc(const Vec3& inFrom, const Vec3& inTo, float inWidth)
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

    void CallMeteor(float inDamage, float inRadius, float inDelay)
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

    void UpdateMeteors(float inDeltaTime)
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

    void Explode(const Vec3& inPosition, float inRadius, float inDamage, EExplosion inStyle, bool inHurtsPlayer, float inPlayerDamage)
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

    void SpawnGem(const Vec2& inPosition, int inValue)
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

    static int GetGemTier(int inValue)
    {
        return inValue >= 25 ? 2 : inValue >= 5 ? 1 : 0;
    }

    void UpdateGems(float inDeltaTime)
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

    void SpawnPickup(int inType, const Vec2& inPosition)
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

    void UpdatePickups(float inDeltaTime)
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

    void CollectPickup(Pickup& ioPickup)
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

    void OpenChest(const Vec2& inPosition)
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

    void AddExperience(int inValue)
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

    void UpdateProgress()
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

    void OpenLevelUp()
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

    void ApplyUpgrade(int inUpgrade)
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

    void ChooseUpgrade(int inChoice)
    {
        if (inChoice < 0 || inChoice >= m_ChoiceCount)
            return;

        ApplyUpgrade(m_Choices[inChoice]);
        PlaySfx(cChooseSound, 0.6f);

        m_ChoiceCount = 0;
        m_State = STATE_PLAYING;
    }

    void Emit(const Vec3& inPosition, const ParticleDesc& inDesc)
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

    void UpdateParticles(float inDeltaTime)
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

    void SpawnNovaBurst(const Vec3& inPosition, float inRadius)
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

    void UpdateNovaBursts(float inDeltaTime)
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

    int SpawnTelegraph(const Vec3& inPosition, float inRadius, float inDuration, Entity inMaterial)
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

    void ReleaseTelegraph(int inIndex)
    {
        if (inIndex >= 0 && inIndex < int(m_Telegraphs.size()))
            ReleaseTelegraph(m_Telegraphs[inIndex]);
    }

    void ReleaseTelegraph(Telegraph& ioTelegraph)
    {
        ioTelegraph.mActive = false;
        Park(ioTelegraph.mDisc);
        Park(ioTelegraph.mRing);
    }

    void UpdateTelegraphs(float inDeltaTime)
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

    void SpawnBeam(const Vec3& inStart, const Vec3& inEnd, float inWidth, float inLife)
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

    void UpdateBeams(float inDeltaTime)
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

    void SpawnLight(const Vec3& inPosition, const Vec3& inColor, float inIntensity, float inRadius, float inLife)
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

    void UpdateLights(float inDeltaTime)
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

    void AnimatePlayer(float inDeltaTime)
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

    void AnimateEnemy(Enemy& ioEnemy, float inDeltaTime)
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

    void AnimateDyingEnemy(Enemy& ioEnemy)
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

    void UpdateVisuals(float inDeltaTime)
    {
        PROFILE_FUNCTION_CPU();

        if (m_State == STATE_PLAYING)
            m_PlayerYaw = LerpAngle(m_PlayerYaw, GetYaw(m_FacingDirection), 1.0f - glm::exp(-inDeltaTime * 14.0f));

        AnimatePlayer(inDeltaTime);

        const bool player_flash = m_InvulnerableTimer > 0.0f && glm::fract(m_Time * 12.0f) < 0.5f;

        if (player_flash)
            SetModelMaterial(m_PlayerModel, m_FlashMaterial);
        else
            RestoreModelMaterials(m_PlayerModel);

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

    void UpdateCamera(float inDeltaTime)
    {
        PROFILE_FUNCTION_CPU();

        Transform* transform = FindComponent<Transform>(m_CameraEntity);

        if (transform == nullptr)
            return;

        const Vec3 lookahead = ToWorld(m_PlayerVelocity * 0.25f, 0.0f);
        const Vec3 player = ToWorld(m_PlayerPosition, 0.8f) + lookahead;
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

        const float pitch = glm::radians(58.0f);
        const float distance = m_CameraDistance - m_CameraKick * shake_scale;
        const Vec3 position = m_CameraTarget + Vec3(0.0f, glm::sin(pitch), glm::cos(pitch)) * distance + offset;

        transform->position = position;
        transform->rotation = glm::angleAxis(roll, glm::normalize(m_CameraTarget - position)) * glm::quatLookAtRH(glm::normalize(m_CameraTarget - position), Camera::cUp);
        transform->Compose();
    }

    void AddDamageNumber(const Vec3& inPosition, int inValue, bool inCrit)
    {
        if (m_DamageNumbers.size() >= cMaxDamageNumbers)
            m_DamageNumbers.erase(m_DamageNumbers.begin());

        m_DamageNumbers.push_back(DamageNumber { .mPosition = inPosition + Vec3(RandomFloat(-0.3f, 0.3f), 0.0f, RandomFloat(-0.3f, 0.3f)), .mTime = 0.0f, .mValue = inValue, .mCrit = inCrit });
    }

    void AddChat(const String& inText, const Vec4& inColor)
    {
        if (m_ChatMessages.size() >= 32)
            m_ChatMessages.erase(m_ChatMessages.begin());

        m_ChatMessages.push_back(ChatMessage { .mText = inText, .mColor = inColor });
    }

    void AddPopup(const String& inText, const String& inSubText, const Vec4& inColor)
    {
        if (m_Popups.size() >= 4)
            m_Popups.erase(m_Popups.begin());

        m_Popups.push_back(Popup { .mText = inText, .mSubText = inSubText, .mTime = 0.0f, .mDuration = 3.0f, .mColor = inColor });
    }

    bool ProjectToScreen(const Vec3& inPosition, Vec2& outScreen) const
    {
        const Viewport& viewport = m_App->GetViewport();
        const Vec4 clip = viewport.GetProjection() * viewport.GetView() * Vec4(inPosition, 1.0f);

        if (clip.w <= 0.0f)
            return false;

        const Vec2 ndc = Vec2(clip) / clip.w;
        outScreen = ( Vec2(ndc.x, -ndc.y) * 0.5f + 0.5f ) * Vec2(viewport.GetDisplaySize());
        return true;
    }

    static Vec4 Fade(const Vec4& inColor, float inAlpha)
    {
        return Vec4(Vec3(inColor), inColor.a * inAlpha);
    }

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

    void DrawLabel(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT)
    {
        g_UIRenderer.AddText(inPos + Vec2(glm::max(1.0f, inSize * 0.08f)), inText, inSize, Vec4(0.0f, 0.0f, 0.0f, inColor.a), inAlign);
        g_UIRenderer.AddText(inPos, inText, inSize, inColor, inAlign);
    }

    void DrawStonePanel(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha = 1.0f)
    {
        g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 6.0f) * inScale, inSize, 8.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.4f * inAlpha), 12.0f * inScale);
        g_UIRenderer.AddImageSliced(inPos, inSize, m_PanelTexture, 18.0f * inScale, 28.0f / 256.0f, Vec4(1.0f, 1.0f, 1.0f, inAlpha));
    }

    void DrawDivider(Vec2 inPos, float inWidth, float inScale, float inAlpha)
    {
        g_UIRenderer.AddRectFilled(inPos, Vec2(inWidth, glm::max(1.0f, inScale)), 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.7f * inAlpha));
        g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, glm::max(1.0f, inScale)), Vec2(inWidth, glm::max(1.0f, inScale)), 0.0f, Vec4(0.55f, 0.48f, 0.38f, 0.6f * inAlpha));
    }

    void DrawWindow(Vec2 inPos, Vec2 inSize, StringView inTitle, float inScale, float inAlpha)
    {
        DrawStonePanel(inPos, inSize, inScale, inAlpha);
        DrawLabel(Vec2(inPos.x + inSize.x * 0.5f, inPos.y + 24.0f * inScale), inTitle, 30.0f * inScale, Fade(cRSOrange, inAlpha), UI_TEXT_ALIGN_CENTER);
        DrawDivider(Vec2(inPos.x + 26.0f * inScale, inPos.y + 64.0f * inScale), inSize.x - 52.0f * inScale, inScale, inAlpha);
    }

    void DrawFlatBar(Vec2 inPos, Vec2 inSize, float inFill, Vec4 inFillColor, Vec4 inBackColor, float inScale)
    {
        g_UIRenderer.AddRectFilled(inPos, inSize, 0.0f, inBackColor);
        g_UIRenderer.AddRectFilled(inPos, Vec2(inSize.x * glm::clamp(inFill, 0.0f, 1.0f), inSize.y), 0.0f, inFillColor);
        g_UIRenderer.AddRect(inPos, inSize, 0.0f, glm::max(1.0f, inScale), Vec4(0.0f, 0.0f, 0.0f, inBackColor.a));
    }

    void DrawSlotBox(Vec2 inPos, Vec2 inSize, float inScale, float inAlpha = 1.0f)
    {
        g_UIRenderer.AddImageSliced(inPos, inSize, m_SlotTexture, 8.0f * inScale, 10.0f / 64.0f, Vec4(1.0f, 1.0f, 1.0f, inAlpha));
    }

    void DrawButton(Vec2 inPos, Vec2 inSize, StringView inLabel, bool inHovered, float inAlpha, float inScale)
    {
        const float tint = inHovered ? 1.35f : 1.0f;
        g_UIRenderer.AddImageSliced(inPos, inSize, m_ButtonTexture, 10.0f * inScale, 12.0f / 64.0f, Vec4(tint, tint, tint, inAlpha));
        DrawLabel(Vec2(inPos.x + inSize.x * 0.5f, inPos.y + inSize.y * 0.5f - 14.0f * inScale), inLabel, 28.0f * inScale, Fade(inHovered ? cRSYellow : cRSOrange, inAlpha), UI_TEXT_ALIGN_CENTER);
    }

    void DrawGem(Vec2 inCenter, float inSize, Vec4 inColor)
    {
        g_UIRenderer.AddImage(inCenter - Vec2(inSize * 0.5f), Vec2(inSize), m_GemTexture, inColor);
    }

    void DrawUpgradeIcon(int inUpgrade, Vec2 inCenter, float inSize, float inAlpha)
    {
        g_UIRenderer.AddImage(inCenter - Vec2(inSize * 1.25f), Vec2(inSize * 2.5f), m_UpgradeIconTextures[inUpgrade], Vec4(1.0f, 1.0f, 1.0f, inAlpha));
    }

    static Vec4 GetOrbValueColor(float inFraction)
    {
        const float fraction = glm::clamp(inFraction, 0.0f, 1.0f);
        return fraction > 0.5f ? glm::mix(cRSYellow, cRSGreen, ( fraction - 0.5f ) * 2.0f) : glm::mix(cRSRed, cRSYellow, fraction * 2.0f);
    }

    void DrawOrb(Vec2 inPos, float inScale, float inFill, const Vec3& inColor, uint32_t inIcon, StringView inValue, float inValueFraction)
    {
        const float k = 0.8f * inScale;
        const Vec2 center = inPos + Vec2(88.0f, 40.0f) * k;
        const float radius = 27.0f * k;
        const float fill = glm::clamp(inFill, 0.0f, 1.0f);

        g_UIRenderer.AddCircleFilled(center, radius, Vec4(0.06f, 0.05f, 0.04f, 1.0f));

        if (fill > 0.0f)
            g_UIRenderer.AddImage(Vec2(center.x - radius, center.y + radius - 2.0f * radius * fill), Vec2(2.0f * radius, 2.0f * radius * fill), m_OrbFillTexture, Vec4(inColor, 1.0f), Vec4(0.0f, 1.0f - fill, 1.0f, 1.0f));

        g_UIRenderer.AddImage(inPos - Vec2(0.0f, 24.0f * k), Vec2(128.0f * k), m_OrbFrameTexture);
        g_UIRenderer.AddImage(center - Vec2(15.0f * k), Vec2(30.0f * k), inIcon);
        DrawLabel(inPos + Vec2(34.0f, 29.0f) * k, inValue, 22.0f * k, GetOrbValueColor(inValueFraction), UI_TEXT_ALIGN_CENTER);
    }

    void DrawHUD(float inDeltaTime)
    {
        PROFILE_FUNCTION_CPU();

        const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
        const float scale = glm::max(display.y / 1080.0f, 0.5f);
        const float margin = 20.0f * scale;

        g_UIRenderer.SetFont(m_HudFont);

        const float health_fraction = m_Health / GetMaxHealth();

        if (m_HitFlash > 0.0f)
            g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 120.0f * scale, Vec4(0.8f, 0.05f, 0.05f, 0.4f * m_HitFlash));

        if (health_fraction < 0.3f && m_State == STATE_PLAYING)
        {
            const float pulse = 0.5f + 0.5f * glm::sin(m_Time * 6.0f);
            g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 90.0f * scale, Vec4(0.7f, 0.0f, 0.05f, ( 0.12f + 0.12f * pulse ) * ( 1.0f - health_fraction / 0.3f )));
        }

        if (m_State == STATE_PLAYING)
        {
            DrawHitsplats(inDeltaTime, scale);
            DrawHintArrows(display, scale);
        }

        Vec2 player_screen;
        if (m_State == STATE_PLAYING && health_fraction < 1.0f && ProjectToScreen(ToWorld(m_PlayerPosition, 2.2f), player_screen))
        {
            const Vec2 size = Vec2(64.0f, 8.0f) * scale;
            DrawFlatBar(player_screen - Vec2(size.x * 0.5f, size.y), size, health_fraction, cHealthGreen, cHealthRed, scale);
        }

        DrawInfoBox(scale, margin);
        DrawExperience(display, inDeltaTime, scale, margin);
        DrawBossBar(display, scale, margin);
        DrawInventory(display, scale, margin);
        DrawChatbox(display, scale, margin);
        DrawPopups(display, inDeltaTime, scale);

        if (m_RunTime < 4.5f && m_State == STATE_PLAYING)
            DrawIntro(display, scale);

        if (m_ScreenFlash > 0.0f)
            g_UIRenderer.AddRectFilled(Vec2(0.0f), display, 0.0f, Vec4(1.0f, 0.97f, 0.9f, m_ScreenFlash * 0.7f));

        if (m_State == STATE_LEVEL_UP)
            DrawLevelUp(display, scale);
        else if (( m_State == STATE_GAME_OVER || m_State == STATE_VICTORY ) && m_PlayerDeathTimer > 0.6f)
            DrawEndScreen(display, scale);
    }

    void DrawInfoBox(float inScale, float inMargin)
    {
        Array<std::pair<String, String>> rows =
        {
            { "Time:", FormatTime(m_RunTime) },
            { "Dawn in:", FormatTime(glm::max(0.0f, cVictoryTime - m_RunTime)) },
            { "Kills:", std::format("{}", m_Kills) },
            { "Damage:", std::format("{:.1f}k", m_DamageDealt / 1000.0f) },
        };

        const bool streak = m_Streak >= 10 && m_State == STATE_PLAYING;

        if (streak)
            rows.push_back({ "Streak:", std::format("x{}", m_Streak) });

        const float row_height = 26.0f * inScale;
        const Vec2 pos = Vec2(inMargin);
        const Vec2 size = Vec2(260.0f * inScale, 92.0f * inScale + float(rows.size()) * row_height);
        const float night = glm::clamp(m_RunTime / cVictoryTime, 0.0f, 1.0f);

        DrawStonePanel(pos, size, inScale, 0.94f);
        DrawLabel(Vec2(pos.x + size.x * 0.5f, pos.y + 18.0f * inScale), "Dusk Swarm", 24.0f * inScale, cRSOrange, UI_TEXT_ALIGN_CENTER);
        DrawFlatBar(pos + Vec2(24.0f, 52.0f) * inScale, Vec2(size.x - 48.0f * inScale, 10.0f * inScale), night, cRSOrange, Vec4(0.1f, 0.08f, 0.18f, 1.0f), inScale);

        for (size_t index = 0; index < rows.size(); index++)
        {
            const float y = pos.y + 72.0f * inScale + float(index) * row_height;
            const bool is_streak = streak && index == rows.size() - 1;
            const Vec4 value_color = is_streak ? Fade(cRSOrange, 0.4f + 0.6f * glm::clamp(m_StreakTimer / cStreakWindow, 0.0f, 1.0f)) : cRSYellow;

            DrawLabel(Vec2(pos.x + 24.0f * inScale, y), rows[index].first, 20.0f * inScale, cRSWhite);
            DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, y), rows[index].second, 20.0f * inScale, value_color, UI_TEXT_ALIGN_RIGHT);
        }
    }

    void DrawExperience(const Vec2& inDisplay, float inDeltaTime, float inScale, float inMargin)
    {
        const Vec2 size = Vec2(240.0f, 104.0f) * inScale;
        const Vec2 pos = Vec2(inDisplay.x - inMargin - size.x, inMargin);
        const int needed = GetExperienceToNextLevel();

        DrawStonePanel(pos, size, inScale, 0.94f);
        DrawLabel(pos + Vec2(24.0f, 20.0f) * inScale, "Level:", 20.0f * inScale, cRSWhite);
        DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, pos.y + 20.0f * inScale), std::format("{}", m_Level), 20.0f * inScale, cRSYellow, UI_TEXT_ALIGN_RIGHT);
        DrawLabel(pos + Vec2(24.0f, 46.0f) * inScale, "XP:", 20.0f * inScale, cRSWhite);
        DrawLabel(Vec2(pos.x + size.x - 24.0f * inScale, pos.y + 46.0f * inScale), std::format("{} / {}", m_Experience, needed), 20.0f * inScale, cRSYellow, UI_TEXT_ALIGN_RIGHT);
        DrawFlatBar(pos + Vec2(24.0f, 76.0f) * inScale, Vec2(size.x - 48.0f * inScale, 10.0f * inScale), float(m_Experience) / float(needed), Vec4(0.2f, 0.65f, 1.0f, 1.0f), Vec4(0.05f, 0.06f, 0.12f, 1.0f), inScale);

        const float orb_width = 128.0f * 0.8f * inScale;
        const Vec2 orb_pos = Vec2(pos.x + size.x - orb_width, pos.y + size.y + 6.0f * inScale);
        const float health_fraction = m_Health / GetMaxHealth();
        const float dash_ready = glm::clamp(1.0f - m_DashCooldownTimer / cDashCooldown, 0.0f, 1.0f);

        DrawOrb(orb_pos, inScale, health_fraction, Vec3(0.78f, 0.06f, 0.05f), m_HeartTexture, std::format("{}", int(glm::ceil(m_Health))), health_fraction);
        DrawOrb(orb_pos + Vec2(0.0f, 68.0f * inScale), inScale, dash_ready, Vec3(0.95f, 0.78f, 0.2f), m_DashIconTexture, std::format("{}", int(dash_ready * 100.0f)), dash_ready);

        const float drop_x = orb_pos.x - 14.0f * inScale;

        for (XPDrop& drop : m_XPDrops)
        {
            drop.mTime += inDeltaTime;

            const float alpha = glm::clamp(( cXPDropDuration - drop.mTime ) / 0.4f, 0.0f, 1.0f);
            const float y = orb_pos.y + 110.0f * inScale - drop.mTime * 80.0f * inScale;
            const String text = std::format("+{}", drop.mValue);
            const float width = g_UIRenderer.MeasureText(text, 20.0f * inScale).x;

            DrawGem(Vec2(drop_x - width - 14.0f * inScale, y + 10.0f * inScale), 20.0f * inScale, Vec4(0.45f, 0.85f, 1.0f, alpha));
            DrawLabel(Vec2(drop_x, y), text, 20.0f * inScale, Fade(cRSWhite, alpha), UI_TEXT_ALIGN_RIGHT);
        }

        std::erase_if(m_XPDrops, [](const XPDrop& inDrop) { return inDrop.mTime >= cXPDropDuration; });
    }

    void DrawBossBar(const Vec2& inDisplay, float inScale, float inMargin)
    {
        if (m_BossEnemy < 0)
            return;

        const Enemy& boss = m_Enemies[m_BossEnemy];

        if (!boss.mActive)
            return;

        const float health = glm::max(boss.mHealth, 0.0f);
        const bool enraged = boss.mHealth < boss.mMaxHealth * 0.5f && !boss.mDying;
        const float appear = glm::clamp(( m_RunTime - cBossTimes[glm::max(m_NextBoss - 1, 0)] ) / 0.8f, 0.0f, 1.0f);
        const Vec2 size = Vec2(480.0f, 90.0f) * inScale;
        const Vec2 pos = Vec2(inDisplay.x * 0.5f - size.x * 0.5f, inMargin);
        const Vec2 bar_pos = pos + Vec2(26.0f, 50.0f) * inScale;
        const Vec2 bar_size = Vec2(size.x - 52.0f * inScale, 20.0f * inScale);

        DrawStonePanel(pos, size, inScale, 0.94f * appear);
        DrawLabel(Vec2(inDisplay.x * 0.5f, pos.y + 18.0f * inScale), enraged ? "The Lich (enraged)" : "The Lich", 24.0f * inScale, Fade(enraged ? cRSRed : cRSYellow, appear), UI_TEXT_ALIGN_CENTER);
        DrawFlatBar(bar_pos, bar_size, health / boss.mMaxHealth, Fade(cHealthGreen, appear), Fade(cHealthRed, appear), inScale);
        DrawLabel(Vec2(inDisplay.x * 0.5f, bar_pos.y + 1.0f * inScale), std::format("{} / {}", int(glm::ceil(health)), int(boss.mMaxHealth)), 17.0f * inScale, Fade(cRSWhite, appear), UI_TEXT_ALIGN_CENTER);
    }

    float GetWeaponCooldownFraction(int inUpgrade) const
    {
        const int level = m_UpgradeLevels[inUpgrade];
        const float haste = GetCooldownMultiplier();

        switch (inUpgrade)
        {
            case UPGRADE_BOLT:      return 1.0f - m_BoltTimer / ( ( 0.95f - 0.1f * ( level - 1 ) ) * haste );
            case UPGRADE_NOVA:      return 1.0f - m_NovaTimer / ( ( 3.6f - 0.35f * ( level - 1 ) ) * haste );
            case UPGRADE_FIREBALL:  return 1.0f - m_FireballTimer / ( ( 2.6f - 0.25f * ( level - 1 ) ) * haste );
            case UPGRADE_LIGHTNING: return 1.0f - m_LightningTimer / ( ( 2.2f - 0.2f * ( level - 1 ) ) * haste );
            case UPGRADE_METEOR:    return 1.0f - m_MeteorTimer / ( ( 5.5f - 0.5f * ( level - 1 ) ) * haste );
            default:                return 1.0f;
        }
    }

    void DrawInventory(const Vec2& inDisplay, float inScale, float inMargin)
    {
        Array<int> items;

        for (int pass = 0; pass < 2; pass++)
        {
            for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
            {
                if (m_UpgradeLevels[upgrade] > 0 && cUpgrades[upgrade].mIsWeapon == ( pass == 0 ))
                    items.push_back(upgrade);
            }
        }

        if (items.empty())
            return;

        constexpr int cColumns = 4;
        const float slot = 58.0f * inScale;
        const float gap = 6.0f * inScale;
        const float padding = 20.0f * inScale;
        const int rows = ( int(items.size()) + cColumns - 1 ) / cColumns;
        const Vec2 size = Vec2(padding * 2.0f + cColumns * slot + ( cColumns - 1 ) * gap, padding * 2.0f + rows * slot + ( rows - 1 ) * gap);
        const Vec2 pos = inDisplay - Vec2(inMargin) - size;

        DrawStonePanel(pos, size, inScale, 0.94f);

        for (size_t index = 0; index < items.size(); index++)
        {
            const int upgrade = items[index];
            const int level = m_UpgradeLevels[upgrade];
            const Vec2 slot_pos = pos + Vec2(padding) + Vec2(float(index % cColumns), float(index / cColumns)) * ( slot + gap );

            DrawSlotBox(slot_pos, Vec2(slot), inScale);
            DrawUpgradeIcon(upgrade, slot_pos + Vec2(slot * 0.5f), slot * 0.3f, 1.0f);

            const float ready = glm::clamp(GetWeaponCooldownFraction(upgrade), 0.0f, 1.0f);

            if (ready < 1.0f)
                g_UIRenderer.AddRectFilled(slot_pos + Vec2(3.0f * inScale), Vec2(slot - 6.0f * inScale, ( slot - 6.0f * inScale ) * ( 1.0f - ready )), 2.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.55f));

            DrawLabel(slot_pos + Vec2(6.0f, 3.0f) * inScale, std::format("{}", level), 19.0f * inScale, level >= cMaxUpgradeLevel ? cRSGreen : cRSYellow);
        }
    }

    void DrawChatbox(const Vec2& inDisplay, float inScale, float inMargin)
    {
        const Vec2 size = Vec2(540.0f, 184.0f) * inScale;
        const Vec2 pos = Vec2(inMargin, inDisplay.y - inMargin - size.y);
        const float text_size = 18.0f * inScale;
        const float line = 22.0f * inScale;
        const float input_y = pos.y + size.y - 34.0f * inScale;
        const float left = pos.x + 16.0f * inScale;

        g_UIRenderer.AddImageSliced(pos, size, m_ParchmentTexture, 14.0f * inScale, 16.0f / 256.0f, Vec4(1.0f, 1.0f, 1.0f, 0.9f));
        g_UIRenderer.AddRectFilled(Vec2(left, input_y - 6.0f * inScale), Vec2(size.x - 32.0f * inScale, glm::max(1.0f, inScale)), 0.0f, Vec4(0.23f, 0.17f, 0.1f, 0.6f));

        const float name_width = g_UIRenderer.AddText(Vec2(left, input_y), "Survivor: ", text_size, cChatBlack);

        if (glm::fract(m_Time * 0.8f) < 0.5f)
            g_UIRenderer.AddText(Vec2(left + name_width, input_y), "*", text_size, cChatBlue);

        int shown = 0;

        for (auto message = m_ChatMessages.rbegin(); message != m_ChatMessages.rend() && shown < cChatLines; ++message, ++shown)
            g_UIRenderer.AddText(Vec2(left, input_y - 10.0f * inScale - line * float(shown + 1)), message->mText, text_size, message->mColor);
    }

    void DrawHintArrows(const Vec2& inDisplay, float inScale)
    {
        const auto DrawHintArrow = [&](const Vec3& inWorld)
        {
            Vec2 screen;
            const bool visible = ProjectToScreen(inWorld, screen);
            const float edge = 50.0f * inScale;

            if (visible && screen.x > edge && screen.y > edge && screen.x < inDisplay.x - edge && screen.y < inDisplay.y - edge)
                return;

            Vec2 player_screen;
            if (!ProjectToScreen(GetPlayerCenter(), player_screen))
                return;

            Vec2 direction = Vec2(inWorld.x - m_PlayerPosition.x, inWorld.z - m_PlayerPosition.y);
            direction = glm::length(direction) > 0.001f ? glm::normalize(direction) : Vec2(1.0f, 0.0f);

            const Vec2 center = inDisplay * 0.5f;
            const Vec2 extent = inDisplay * 0.5f - Vec2(edge);
            const float t = glm::min(extent.x / glm::max(glm::abs(direction.x), 0.001f), extent.y / glm::max(glm::abs(direction.y), 0.001f));
            const Vec2 position = center + direction * ( t - 6.0f * inScale * ( 1.0f + glm::sin(m_Time * 7.0f) ) );

            g_UIRenderer.AddImageRotated(position, Vec2(48.0f * inScale), glm::atan(direction.y, direction.x), m_HintArrowTexture);
        };

        for (const Pickup& pickup : m_Pickups)
        {
            if (pickup.mActive && pickup.mType == PICKUP_CHEST)
                DrawHintArrow(ToWorld(pickup.mPosition, 0.5f));
        }

        if (m_BossEnemy >= 0 && m_Enemies[m_BossEnemy].mActive)
            DrawHintArrow(ToWorld(m_Enemies[m_BossEnemy].mPosition, 1.5f));
    }

    static String FormatTime(float inSeconds)
    {
        const int seconds = int(inSeconds);
        return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
    }

    void DrawHitsplats(float inDeltaTime, float inScale)
    {
        for (DamageNumber& number : m_DamageNumbers)
        {
            number.mTime += inDeltaTime;

            Vec2 screen;
            if (!ProjectToScreen(number.mPosition + Vec3(0.0f, 0.6f + number.mTime * 0.4f, 0.0f), screen))
                continue;

            const float alpha = glm::clamp(( 0.75f - number.mTime ) / 0.2f, 0.0f, 1.0f);
            const float pop = 1.0f + 0.35f * glm::max(0.0f, 1.0f - number.mTime * 10.0f);
            const float size = ( number.mCrit ? 44.0f : 34.0f ) * inScale * pop;
            const float text_size = 19.0f * inScale * pop;

            g_UIRenderer.AddImage(screen - Vec2(size * 0.5f), Vec2(size), number.mCrit ? m_HitsplatMaxTexture : m_HitsplatTexture, Vec4(1.0f, 1.0f, 1.0f, alpha));
            DrawLabel(screen - Vec2(0.0f, text_size * 0.55f), std::format("{}", number.mValue), text_size, Fade(cRSWhite, alpha), UI_TEXT_ALIGN_CENTER);
        }

        std::erase_if(m_DamageNumbers, [](const DamageNumber& inNumber) { return inNumber.mTime >= 0.75f; });
    }

    void DrawPopups(const Vec2& inDisplay, float inDeltaTime, float inScale)
    {
        float popup_y = inDisplay.y * 0.24f;

        for (Popup& popup : m_Popups)
        {
            popup.mTime += inDeltaTime;

            const float fade_in = glm::clamp(popup.mTime / 0.15f, 0.0f, 1.0f);
            const float fade_out = glm::clamp(( popup.mDuration - popup.mTime ) / 0.4f, 0.0f, 1.0f);
            const float alpha = fade_in * fade_out;
            const float size = glm::mix(34.0f, 44.0f, EaseOutBack(fade_in)) * inScale;
            const float center = inDisplay.x * 0.5f;

            DrawLabel(Vec2(center, popup_y), popup.mText, size, Fade(popup.mColor, alpha), UI_TEXT_ALIGN_CENTER);
            popup_y += size * 1.15f;

            if (!popup.mSubText.empty())
            {
                DrawLabel(Vec2(center, popup_y), popup.mSubText, 22.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);
                popup_y += 28.0f * inScale;
            }

            popup_y += 18.0f * inScale;
        }

        std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= inPopup.mDuration; });
    }

    void DrawIntro(const Vec2& inDisplay, float inScale)
    {
        const float alpha = glm::clamp(m_RunTime / 0.5f, 0.0f, 1.0f) * glm::clamp(( 4.5f - m_RunTime ) / 1.0f, 0.0f, 1.0f);
        const float center = inDisplay.x * 0.5f;
        const float y = inDisplay.y * 0.2f;

        g_UIRenderer.SetFont(m_TitleFont);
        DrawLabel(Vec2(center, y), "Dusk Swarm", 100.0f * inScale, Fade(cRSOrange, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.SetFont(m_HudFont);
        DrawLabel(Vec2(center, y + 124.0f * inScale), "Survive until dawn", 32.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);
    }

    void DrawLevelUp(const Vec2& inDisplay, float inScale)
    {
        const float time = m_Time - m_LevelUpTime;
        const float alpha = glm::clamp(time / 0.25f, 0.0f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

        const Vec2 size = Vec2(900.0f, 520.0f) * inScale;
        const Vec2 pos = inDisplay * 0.5f - size * 0.5f + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 30.0f * inScale);
        const float center = inDisplay.x * 0.5f;

        DrawWindow(pos, size, "Level Up!", inScale, alpha);
        DrawLabel(Vec2(center, pos.y + 80.0f * inScale), std::format("Congratulations, you are now level {}. Choose a power:", m_Level), 21.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);

        const Vec2 box_size = Vec2(264.0f, 334.0f) * inScale;
        const float gap = 22.0f * inScale;
        const float total_width = m_ChoiceCount * box_size.x + ( m_ChoiceCount - 1 ) * gap;

        for (int choice = 0; choice < m_ChoiceCount; choice++)
        {
            const int upgrade = m_Choices[choice];
            const UpgradeInfo& info = cUpgrades[upgrade];
            const int level = m_UpgradeLevels[upgrade];
            const bool selected = choice == m_SelectedChoice;
            const float card_alpha = glm::clamp(( time - 0.06f * choice ) / 0.2f, 0.0f, 1.0f) * alpha;
            const Vec2 box_pos = Vec2(center - total_width * 0.5f + choice * ( box_size.x + gap ), pos.y + 120.0f * inScale - ( selected ? 6.0f * inScale : 0.0f ));
            const float box_center = box_pos.x + box_size.x * 0.5f;

            DrawSlotBox(box_pos, box_size, inScale, card_alpha);

            if (selected)
            {
                const float pulse = 0.5f + 0.5f * glm::sin(m_Time * 5.0f);
                g_UIRenderer.AddRectFilled(box_pos, box_size, 4.0f * inScale, Vec4(1.0f, 1.0f, 0.8f, 0.06f * card_alpha));
                g_UIRenderer.AddRect(box_pos - Vec2(2.0f * inScale), box_size + Vec2(4.0f * inScale), 4.0f * inScale, 2.0f * inScale, Fade(cRSYellow, ( 0.55f + 0.45f * pulse ) * card_alpha));
            }

            DrawLabel(box_pos + Vec2(14.0f, 10.0f) * inScale, std::format("{}", choice + 1), 22.0f * inScale, Fade(cRSYellow, card_alpha));
            DrawUpgradeIcon(upgrade, Vec2(box_center, box_pos.y + 78.0f * inScale), 32.0f * inScale, card_alpha);
            DrawLabel(Vec2(box_center, box_pos.y + 140.0f * inScale), info.mName, 25.0f * inScale, Fade(cRSOrange, card_alpha), UI_TEXT_ALIGN_CENTER);

            const bool is_new = upgrade != UPGRADE_FEAST && level == 0;
            const String chip = upgrade == UPGRADE_FEAST ? String("Restores health") : is_new ? String(info.mIsWeapon ? "New weapon!" : "New!") : std::format("Level {} -> {}", level, level + 1);
            DrawLabel(Vec2(box_center, box_pos.y + 174.0f * inScale), chip, 19.0f * inScale, Fade(is_new ? cRSGreen : cRSYellow, card_alpha), UI_TEXT_ALIGN_CENTER);
            DrawLabel(Vec2(box_center, box_pos.y + 212.0f * inScale), info.mDescription, 19.0f * inScale, Fade(cRSWhite, card_alpha), UI_TEXT_ALIGN_CENTER);

            if (upgrade == UPGRADE_FEAST)
                continue;

            for (int pip = 0; pip < cMaxUpgradeLevel; pip++)
            {
                const Vec2 pip_size = Vec2(16.0f * inScale);
                const Vec2 pip_pos = Vec2(box_center + ( float(pip) - 2.5f ) * 24.0f * inScale + 4.0f * inScale, box_pos.y + box_size.y - 40.0f * inScale);
                const bool next = pip == level;
                const float blink = 0.5f + 0.5f * glm::sin(m_Time * 6.0f);
                const Vec4 color = pip < level ? cRSOrange : next ? Fade(cRSYellow, blink) : Vec4(0.12f, 0.1f, 0.08f, 1.0f);

                g_UIRenderer.AddRectFilled(pip_pos, pip_size, 2.0f * inScale, Fade(color, card_alpha));
                g_UIRenderer.AddRect(pip_pos, pip_size, 2.0f * inScale, glm::max(1.0f, inScale), Vec4(0.0f, 0.0f, 0.0f, card_alpha));
            }
        }

        DrawLabel(Vec2(center, pos.y + size.y - 50.0f * inScale), "Press 1, 2 or 3 to choose, or A / D and Enter", 19.0f * inScale, Fade(cRSWhite, 0.85f * alpha), UI_TEXT_ALIGN_CENTER);
    }

    void DrawEndScreen(const Vec2& inDisplay, float inScale)
    {
        const bool victory = m_State == STATE_VICTORY;
        const float alpha = glm::clamp(( m_PlayerDeathTimer - 0.6f ) / 0.6f, 0.0f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

        const Vec2 size = Vec2(620.0f, 430.0f) * inScale;
        const Vec2 pos = inDisplay * 0.5f - size * 0.5f + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 40.0f * inScale);
        const float center = inDisplay.x * 0.5f;

        DrawWindow(pos, size, victory ? "Congratulations!" : "Oh dear, you are dead!", inScale, alpha);
        DrawLabel(Vec2(center, pos.y + 80.0f * inScale), victory ? "You survived until dawn. The swarm retreats." : "The night claims another soul.", 21.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_CENTER);

        const StaticArray<std::pair<String, String>, 6> stats =
        {
            std::pair { String("Survived:"), FormatTime(m_RunTime) },
            std::pair { String("Level:"), std::format("{}", m_Level) },
            std::pair { String("Kills:"), std::format("{}", m_Kills) },
            std::pair { String("Best streak:"), std::format("{}", m_BestStreak) },
            std::pair { String("Elites slain:"), std::format("{}", m_EliteKills) },
            std::pair { String("Damage:"), std::format("{:.1f}k", m_DamageDealt / 1000.0f) },
        };

        const float column_width = ( size.x - 100.0f * inScale ) * 0.5f;

        for (size_t index = 0; index < stats.size(); index++)
        {
            const float x = pos.x + 40.0f * inScale + float(index % 2) * ( column_width + 20.0f * inScale );
            const float y = pos.y + 128.0f * inScale + float(index / 2) * 40.0f * inScale;

            DrawLabel(Vec2(x, y), stats[index].first, 22.0f * inScale, Fade(cRSWhite, alpha));
            DrawLabel(Vec2(x + column_width, y), stats[index].second, 22.0f * inScale, Fade(cRSYellow, alpha), UI_TEXT_ALIGN_RIGHT);
        }

        DrawDivider(Vec2(pos.x + 26.0f * inScale, pos.y + 262.0f * inScale), size.x - 52.0f * inScale, inScale, alpha);
        DrawLabel(Vec2(center, pos.y + 282.0f * inScale), std::format("Personal best: {}", FormatTime(m_BestTime)), 22.0f * inScale, Fade(cRSOrange, alpha), UI_TEXT_ALIGN_CENTER);

        const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
        DrawLabel(Vec2(center, pos.y + size.y - 70.0f * inScale), "Press Enter to play again", 26.0f * inScale, Fade(cRSYellow, alpha * blink), UI_TEXT_ALIGN_CENTER);
    }

    struct PauseLayout
    {
        float mScale = 1.0f;
        Vec2 mDisplay = Vec2(0.0f);
        Vec2 mPanelPos = Vec2(0.0f);
        Vec2 mPanelSize = Vec2(0.0f);
        Vec2 mSliderPos = Vec2(0.0f);
        Vec2 mSliderSize = Vec2(0.0f);
        Vec2 mSliderPadding = Vec2(0.0f);
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
    };

    PauseLayout GetPauseLayout() const
    {
        PauseLayout layout;
        layout.mDisplay = Vec2(m_App->GetViewport().GetDisplaySize());
        layout.mScale = glm::max(layout.mDisplay.y / 1080.0f, 0.5f);

        const float scale = layout.mScale;
        layout.mPanelSize = Vec2(620.0f, 540.0f) * scale;
        layout.mPanelPos = layout.mDisplay * 0.5f - layout.mPanelSize * 0.5f + Vec2(0.0f, 20.0f * scale);

        layout.mTabSize = Vec2(200.0f, 44.0f) * scale;
        const float tab_gap = 12.0f * scale;
        const float tabs_x = layout.mDisplay.x * 0.5f - ( layout.mTabSize.x * float(PAUSE_TAB_COUNT) + tab_gap * float(PAUSE_TAB_COUNT - 1) ) * 0.5f;

        for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
            layout.mTabPos[tab] = Vec2(tabs_x + ( layout.mTabSize.x + tab_gap ) * float(tab), layout.mPanelPos.y + 84.0f * scale);

        layout.mSliderSize = Vec2(380.0f, 16.0f) * scale;
        layout.mSliderPos = Vec2(layout.mDisplay.x * 0.5f - layout.mSliderSize.x * 0.5f - 40.0f * scale, layout.mPanelPos.y + 200.0f * scale);
        layout.mSliderPadding = Vec2(16.0f, 18.0f) * scale;
        layout.mCheckboxSize = Vec2(360.0f, 32.0f) * scale;
        layout.mGodModePos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 250.0f * scale);
        layout.mSettingsPos = Vec2(layout.mSliderPos.x, layout.mPanelPos.y + 148.0f * scale);
        layout.mSettingStep = 44.0f * scale;
        layout.mButtonSize = Vec2(240.0f, 56.0f) * scale;
        layout.mButtonPos = Vec2(layout.mDisplay.x * 0.5f - layout.mButtonSize.x * 0.5f, layout.mPanelPos.y + 380.0f * scale);
        layout.mQuitPos = layout.mButtonPos + Vec2(0.0f, layout.mButtonSize.y + 16.0f * scale);
        return layout;
    }

    static bool IsInside(const Vec2& inPoint, const Vec2& inPos, const Vec2& inSize)
    {
        return inPoint.x >= inPos.x && inPoint.y >= inPos.y && inPoint.x <= inPos.x + inSize.x && inPoint.y <= inPos.y + inSize.y;
    }

    float GetMasterVolume()
    {
        Audio* audio = GetAudio();
        return audio ? glm::clamp(audio->GetMasterVolume(), 0.0f, 1.0f) : 0.0f;
    }

    void SetMasterVolume(float inVolume)
    {
        if (Audio* audio = GetAudio())
            audio->SetMasterVolume(glm::clamp(glm::round(inVolume * 100.0f) / 100.0f, 0.0f, 1.0f));
    }

    void SetVolumeFromMouse(const PauseLayout& inLayout)
    {
        SetMasterVolume(( m_MousePosition.x - inLayout.mSliderPos.x ) / inLayout.mSliderSize.x);
    }

    void ToggleGodMode()
    {
        *m_GodMode = *m_GodMode ? 0 : 1;
        PlaySfx(cSelectSound, 0.5f);
    }

    static int* GetGraphicsSetting(int inSetting)
    {
        return g_CVariables->TryGetValue<int>(cGraphicsSettings[inSetting].mCVar);
    }

    void ToggleGraphicsSetting(int inSetting)
    {
        if (int* value = GetGraphicsSetting(inSetting))
        {
            *value = *value ? 0 : 1;
            PlaySfx(cSelectSound, 0.5f);
        }
    }

    void SetPauseTab(int inTab)
    {
        if (inTab == m_PauseTab)
            return;

        m_PauseTab = inTab;
        m_DraggingVolume = false;
        PlaySfx(cSelectSound, 0.5f);
    }

    void Resume()
    {
        m_DraggingVolume = false;

        if (Game* game = dynamic_cast<Game*>(m_App))
            game->Unpause();
    }

    void Quit()
    {
        m_DraggingVolume = false;

        if (Game* game = dynamic_cast<Game*>(m_App))
            game->Quit();
    }

    void DrawPauseMenu()
    {
        const PauseLayout layout = GetPauseLayout();
        const float scale = layout.mScale;
        const float alpha = glm::clamp(m_PauseTime / 0.2f, 0.0f, 1.0f);
        const float center = layout.mDisplay.x * 0.5f;
        const Vec2 pos = layout.mPanelPos + Vec2(0.0f, ( 1.0f - EaseOutCubic(alpha) ) * 30.0f * scale);
        const Vec2 offset = pos - layout.mPanelPos;

        g_UIRenderer.SetFont(m_HudFont);
        g_UIRenderer.AddRectFilled(Vec2(0.0f), layout.mDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.0f, 0.55f * alpha));

        DrawWindow(pos, layout.mPanelSize, "Paused", scale, alpha);

        for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
        {
            const bool active = tab == m_PauseTab;
            const bool hovered = IsInside(m_MousePosition, layout.mTabPos[tab], layout.mTabSize);
            const float tint = active ? 1.35f : hovered ? 1.1f : 0.7f;
            const Vec2 tab_pos = layout.mTabPos[tab] + offset;

            g_UIRenderer.AddImageSliced(tab_pos, layout.mTabSize, m_ButtonTexture, 10.0f * scale, 12.0f / 64.0f, Vec4(tint, tint, tint, alpha));
            DrawLabel(Vec2(tab_pos.x + layout.mTabSize.x * 0.5f, tab_pos.y + layout.mTabSize.y * 0.5f - 12.0f * scale), cPauseTabNames[tab], 24.0f * scale, Fade(active || hovered ? cRSYellow : cRSOrange, ( active ? 1.0f : 0.75f ) * alpha), UI_TEXT_ALIGN_CENTER);
        }

        if (m_PauseTab == PAUSE_TAB_GENERAL)
            DrawPauseGeneralTab(layout, offset, alpha);
        else
            DrawPauseGraphicsTab(layout, offset, alpha);

        DrawButton(layout.mButtonPos + offset, layout.mButtonSize, "Resume", IsInside(m_MousePosition, layout.mButtonPos, layout.mButtonSize), alpha, scale);
        DrawButton(layout.mQuitPos + offset, layout.mButtonSize, "Quit", IsInside(m_MousePosition, layout.mQuitPos, layout.mButtonSize), alpha, scale);

        const char* hint = m_PauseTab == PAUSE_TAB_GENERAL ? "A / D or drag to adjust  -  G for god mode  -  Tab to switch  -  Esc to resume  -  Q to quit" : "Click to toggle  -  Tab to switch  -  Esc to resume  -  Q to quit";
        DrawLabel(Vec2(center, pos.y + layout.mPanelSize.y + 20.0f * scale), hint, 20.0f * scale, Fade(cRSWhite, 0.85f * alpha), UI_TEXT_ALIGN_CENTER);
    }

    void DrawCheckbox(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inChecked, float inAlpha, float inScale)
    {
        const bool hovered = IsInside(m_MousePosition, inLayoutPos, inSize);
        const Vec2 pos = inLayoutPos + inOffset;
        const Vec2 box_size = Vec2(inSize.y);

        DrawSlotBox(pos, box_size, inScale, inAlpha);

        if (inChecked)
            g_UIRenderer.AddRectFilled(pos + Vec2(7.0f * inScale), box_size - Vec2(14.0f * inScale), 2.0f * inScale, Fade(cRSOrange, 0.9f * inAlpha));

        DrawLabel(pos + Vec2(box_size.x + 14.0f * inScale, 4.0f * inScale), inLabel, 22.0f * inScale, Fade(hovered ? cRSYellow : cRSWhite, inAlpha));
    }

    void DrawPauseGraphicsTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
    {
        for (int setting = 0; setting < int(cGraphicsSettings.size()); setting++)
        {
            const int* value = GetGraphicsSetting(setting);
            DrawCheckbox(inLayout.GetSettingPos(setting), inLayout.mCheckboxSize, inOffset, cGraphicsSettings[setting].mLabel, value && *value != 0, inAlpha, inLayout.mScale);
        }
    }

    void DrawPauseGeneralTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha)
    {
        const float scale = inLayout.mScale;

        const float volume = GetMasterVolume();
        const Vec2 slider_pos = inLayout.mSliderPos + inOffset;
        const Vec2 slider_size = inLayout.mSliderSize;
        const bool slider_hovered = m_DraggingVolume || IsInside(m_MousePosition, inLayout.mSliderPos - inLayout.mSliderPadding, slider_size + inLayout.mSliderPadding * 2.0f);

        DrawLabel(Vec2(slider_pos.x, slider_pos.y - 44.0f * scale), "Master volume", 22.0f * scale, Fade(cRSWhite, inAlpha));
        DrawSlotBox(slider_pos - Vec2(4.0f * scale), slider_size + Vec2(8.0f * scale), scale, inAlpha);
        g_UIRenderer.AddRectFilled(slider_pos, Vec2(slider_size.x * volume, slider_size.y), 2.0f * scale, Fade(cRSOrange, 0.85f * inAlpha));

        const Vec2 knob_size = Vec2(22.0f, slider_size.y / scale + 22.0f) * scale;
        const float knob_tint = slider_hovered ? 1.35f : 1.0f;
        g_UIRenderer.AddImageSliced(Vec2(slider_pos.x + slider_size.x * volume - knob_size.x * 0.5f, slider_pos.y - 11.0f * scale), knob_size, m_ButtonTexture, 8.0f * scale, 12.0f / 64.0f, Vec4(knob_tint, knob_tint, knob_tint, inAlpha));

        DrawLabel(Vec2(slider_pos.x + slider_size.x + 70.0f * scale, slider_pos.y - 6.0f * scale), std::format("{}%", int(glm::round(volume * 100.0f))), 26.0f * scale, Fade(cRSYellow, inAlpha), UI_TEXT_ALIGN_CENTER);

        DrawCheckbox(inLayout.mGodModePos, inLayout.mCheckboxSize, inOffset, "God mode", *m_GodMode != 0, inAlpha, scale);
    }

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
    StaticArray<Node, cMaxBlades> m_Blades;

    Array<SceneNode> m_Nodes;
    Array<MeshBatch> m_MeshBatches;
    Array<MeshInstanced*> m_MeshBatchComponents;

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
    bool m_DraggingVolume = false;

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
    float m_Trauma = 0.0f;
    float m_HitStop = 0.0f;
    float m_HitFlash = 0.0f;
    float m_ScreenFlash = 0.0f;

    int* m_GodMode = nullptr;
    int* m_AutoPick = nullptr;
    float* m_StartTime = nullptr;
    int* m_StartWeapons = nullptr;
    float* m_ShakeScale = nullptr;

public:
    DuskSwarmScript()
    {
        m_Blades.fill(Node::Null);
        m_GemMaterials.fill(Entity::Null);
        m_FXMaterials.fill(Entity::Null);
        m_GroundMaterials.fill(Entity::Null);
    }
};


RTTI_DEFINE_TYPE(DuskSwarmScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(DuskSwarmScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(DuskSwarmScript, SERIALIZE_ALL, "Best Time", m_BestTime);
    RTTI_DEFINE_SCRIPT_MEMBER(DuskSwarmScript, SERIALIZE_ALL, "Camera Distance", m_CameraDistance);
}

RK_REGISTER_SCRIPT(DuskSwarmScript)

} // RK
