#pragma once

#include "../Engine/Raekor.h"
#include "../Engine/Profiler.h"
#include "../Engine/GLTF.h"
#include "../MeshNodes.h"

namespace RK {

inline constexpr float cArenaHalfSize = 40.0f;
inline constexpr float cPlayerRadius = 0.45f;
inline constexpr float cPlayerBaseSpeed = 6.0f;
inline constexpr float cPlayerBaseHealth = 100.0f;
inline constexpr float cPlayerInvulnerability = 0.6f;
inline constexpr float cBaseMagnetRadius = 2.5f;
inline constexpr float cSpawnDistance = 26.0f;
inline constexpr float cMinSpawnDistance = 15.0f;
inline constexpr float cFlashDuration = 0.08f;
inline constexpr float cVictoryTime = 600.0f;
inline constexpr float cSwarmInterval = 90.0f;
inline constexpr float cBruteInterval = 60.0f;
inline constexpr float cDashSpeed = 26.0f;
inline constexpr float cDashDuration = 0.2f;
inline constexpr float cDashCooldown = 2.2f;
inline constexpr float cMaxEnemyRadius = 1.3f;
inline constexpr float cEliteChance = 0.035f;
inline constexpr float cStreakWindow = 2.5f;
inline constexpr float cXPDropDuration = 1.4f;
inline constexpr int cChatLines = 6;
inline constexpr float cAmbientTraumaCap = 0.25f;
inline constexpr float cMaxCameraKick = 0.35f;

inline constexpr StaticArray<float, 2> cBossTimes = { 270.0f, 540.0f };

inline constexpr float cGridCellSize = 2.0f;
inline constexpr int   cGridDim = int(( cArenaHalfSize * 2.0f + 4.0f ) / cGridCellSize) + 1;

inline constexpr int cProjectilePoolSize = 80;
inline constexpr int cFireballPoolSize = 16;
inline constexpr int cArrowPoolSize = 60;
inline constexpr int cOrbPoolSize = 140;
inline constexpr int cGemPoolSize = 300;
inline constexpr int cMeteorPoolSize = 12;
inline constexpr int cNovaBurstPoolSize = 4;
inline constexpr float cNovaBurstLife = 0.5f;
inline constexpr int cTelegraphPoolSize = 24;
inline constexpr int cBeamPoolSize = 120;
inline constexpr int cLightPoolSize = 6;
inline constexpr int cEliteAuraPoolSize = 8;
inline constexpr int cPickupsPerType = 4;
inline constexpr int cMaxBlades = 6;
inline constexpr int cMaxUpgradeLevel = 5;
inline constexpr int cMaxDamageNumbers = 80;
inline constexpr int cChoiceCount = 3;

inline constexpr Vec3 cParkedPosition = Vec3(0.0f, -60.0f, 0.0f);

inline constexpr const char* cRuntimeRootName = "DuskSwarm Runtime";
inline constexpr const char* cModelFile = "Assets/DuskSwarm/DuskSwarm.glb";
inline constexpr const char* cPaletteFile = "Assets/DuskSwarm/Palette.png";
inline constexpr const char* cPaletteEmissiveFile = "Assets/DuskSwarm/PaletteEmissive.png";
inline constexpr const char* cHudFontFile = "Assets/Fonts/PixelifySans.ttf";
inline constexpr const char* cTitleFontFile = "Assets/Fonts/CinzelDecorative-Bold.ttf";

inline constexpr uint8_t cNormalMapSwizzleAG = 0b11'10'01'11;

inline constexpr const char* cMusicSound = "Assets/DuskSwarm/Audio/Music.ogg";
inline constexpr const char* cBossMusicSound = "Assets/DuskSwarm/Audio/BossMusic.ogg";
inline constexpr const char* cAmbienceSound = "Assets/DuskSwarm/Audio/Ambience.ogg";
inline constexpr const char* cBoltSound = "Assets/DuskSwarm/Audio/Bolt.ogg";
inline constexpr const char* cBoltHitSound = "Assets/DuskSwarm/Audio/BoltHit.ogg";
inline constexpr const char* cBladeHitSound = "Assets/DuskSwarm/Audio/BladeHit.ogg";
inline constexpr const char* cEnemyHitSound = "Assets/DuskSwarm/Audio/EnemyHit.ogg";
inline constexpr const char* cNovaSound = "Assets/DuskSwarm/Audio/Nova.ogg";
inline constexpr const char* cFireballCastSound = "Assets/DuskSwarm/Audio/FireballCast.ogg";
inline constexpr const char* cExplosionSound = "Assets/DuskSwarm/Audio/Explosion.ogg";
inline constexpr const char* cMeteorImpactSound = "Assets/DuskSwarm/Audio/MeteorImpact.ogg";
inline constexpr const char* cLightningSound = "Assets/DuskSwarm/Audio/Lightning.ogg";
inline constexpr const char* cPoisonBurstSound = "Assets/DuskSwarm/Audio/PoisonBurst.ogg";
inline constexpr const char* cFuseSound = "Assets/DuskSwarm/Audio/Fuse.ogg";
inline constexpr const char* cHolyBlastSound = "Assets/DuskSwarm/Audio/HolyBlast.ogg";
inline constexpr const char* cSlamSound = "Assets/DuskSwarm/Audio/Slam.ogg";
inline constexpr const char* cBruteRoarSound = "Assets/DuskSwarm/Audio/BruteRoar.ogg";
inline constexpr const char* cArrowShotSound = "Assets/DuskSwarm/Audio/ArrowShot.ogg";
inline constexpr const char* cLichCastSound = "Assets/DuskSwarm/Audio/LichCast.ogg";
inline constexpr const char* cLichRiseSound = "Assets/DuskSwarm/Audio/LichRise.ogg";
inline constexpr const char* cSwarmSound = "Assets/DuskSwarm/Audio/Swarm.ogg";
inline constexpr const char* cPlayerHurtSound = "Assets/DuskSwarm/Audio/PlayerHurt.ogg";
inline constexpr const char* cPlayerDeathSound = "Assets/DuskSwarm/Audio/PlayerDeath.ogg";
inline constexpr const char* cDashSound = "Assets/DuskSwarm/Audio/Dash.ogg";
inline constexpr const char* cGemSound = "Assets/DuskSwarm/Audio/Gem.ogg";
inline constexpr const char* cLevelUpSound = "Assets/DuskSwarm/Audio/LevelUp.ogg";
inline constexpr const char* cSelectSound = "Assets/DuskSwarm/Audio/Select.ogg";
inline constexpr const char* cChooseSound = "Assets/DuskSwarm/Audio/Choose.ogg";
inline constexpr const char* cPotionSound = "Assets/DuskSwarm/Audio/Potion.ogg";
inline constexpr const char* cMagnetSound = "Assets/DuskSwarm/Audio/Magnet.ogg";
inline constexpr const char* cChestOpenSound = "Assets/DuskSwarm/Audio/ChestOpen.ogg";
inline constexpr const char* cTreasureSound = "Assets/DuskSwarm/Audio/Treasure.ogg";
inline constexpr const char* cVictorySound = "Assets/DuskSwarm/Audio/Victory.ogg";
inline constexpr const char* cGameOverSound = "Assets/DuskSwarm/Audio/GameOver.ogg";
inline constexpr const char* cGhoulDeathSound = "Assets/DuskSwarm/Audio/GhoulDeath.ogg";
inline constexpr const char* cBatDeathSound = "Assets/DuskSwarm/Audio/BatDeath.ogg";
inline constexpr const char* cBoneShatterSound = "Assets/DuskSwarm/Audio/BoneShatter.ogg";
inline constexpr const char* cWraithDeathSound = "Assets/DuskSwarm/Audio/WraithDeath.ogg";
inline constexpr const char* cBruteDeathSound = "Assets/DuskSwarm/Audio/BruteDeath.ogg";
inline constexpr const char* cLichDeathSound = "Assets/DuskSwarm/Audio/LichDeath.ogg";

inline constexpr StaticArray<const char*, 37> cSoundEffects =
{
    cBoltSound, cBoltHitSound, cBladeHitSound, cEnemyHitSound, cNovaSound, cFireballCastSound, cExplosionSound,
    cMeteorImpactSound, cLightningSound, cPoisonBurstSound, cFuseSound, cHolyBlastSound, cSlamSound, cBruteRoarSound,
    cArrowShotSound, cLichCastSound, cLichRiseSound, cSwarmSound, cPlayerHurtSound, cPlayerDeathSound, cDashSound,
    cGemSound, cLevelUpSound, cSelectSound, cChooseSound, cPotionSound, cMagnetSound, cChestOpenSound,
    cTreasureSound, cVictorySound, cGameOverSound, cGhoulDeathSound, cBatDeathSound, cBoneShatterSound,
    cWraithDeathSound, cBruteDeathSound, cLichDeathSound
};

inline constexpr float cListenerHeight = 6.0f;
inline constexpr float cSoundMinDistance = 8.0f;
inline constexpr float cGemStreakTime = 0.35f;
inline constexpr int   cGemStreakSteps = 12;
inline constexpr float cGameOverJingleDelay = 1.6f;
inline constexpr float cVolumeStep = 0.05f;
inline constexpr float cEyeHeight = 1.6f;
inline constexpr float cMouseSensitivity = 0.0025f;
inline constexpr float cMinSensitivityScale = 0.2f;
inline constexpr float cMaxSensitivityScale = 3.0f;
inline constexpr float cMaxLookPitch = 1.4f;
inline constexpr float cThirdPersonFov = 50.0f;
inline constexpr float cFirstPersonFov = 75.0f;
inline constexpr float cWandScale = 0.5f;
inline constexpr Vec3 cWandOffset = Vec3(0.21f, -0.22f, -0.52f) * cWandScale;
inline constexpr Vec3 cWandTip = Vec3(0.0f, 0.0f, -0.44f) * cWandScale;
inline constexpr float cWandYaw = 0.06f;
inline constexpr float cWandPitch = 0.12f;
inline constexpr float cBoltSpeed = 17.0f;
inline constexpr float cAimedBoltSpeed = 24.0f;
inline constexpr float cAimedBoltCooldownScale = 0.5f;


struct SkyKey
{
    float mTime;
    float mAngle;
};

inline constexpr StaticArray<SkyKey, 5> cSkyKeys =
{
    SkyKey { 0.0f,   2.4f },
    SkyKey { 70.0f,  glm::pi<float>() },
    SkyKey { 550.0f, glm::two_pi<float>() },
    SkyKey { 600.0f, glm::two_pi<float>() + 0.2f },
    SkyKey { 650.0f, glm::two_pi<float>() + 0.6f },
};

inline constexpr float cSkyInclination = 0.45f;
inline constexpr float cSkyFadeHeight = 0.12f;
inline constexpr float cHorizonSkyBrightness = 0.35f;
inline constexpr float cHorizonIlluminance = 0.15f;
inline constexpr Vec3 cSunsetColor = Vec3(1.0f, 0.36f, 0.14f);
inline constexpr Vec3 cSunColor = Vec3(1.0f, 0.62f, 0.38f);
inline constexpr Vec3 cMoonColor = Vec3(0.08f, 0.11f, 0.22f);
inline constexpr float cSunIlluminance = 60000.0f;
inline constexpr float cMoonIlluminance = 12000.0f;


enum EPauseTab
{
    PAUSE_TAB_GENERAL,
    PAUSE_TAB_GRAPHICS,
    PAUSE_TAB_COUNT
};

inline constexpr StaticArray<const char*, PAUSE_TAB_COUNT> cPauseTabNames = { "General", "Graphics" };


enum ESlider
{
    SLIDER_NONE = -1,
    SLIDER_VOLUME,
    SLIDER_SENSITIVITY,
    SLIDER_COUNT
};

inline constexpr StaticArray<const char*, SLIDER_COUNT> cSliderNames = { "Master volume", "Mouse sensitivity" };


struct GraphicsSetting
{
    const char* mCVar;
    const char* mLabel;
};

inline constexpr StaticArray<GraphicsSetting, 5> cGraphicsSettings =
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


inline const StaticArray<EnemyArchetype, ENEMY_TYPE_COUNT> cEnemyArchetypes =
{
    EnemyArchetype { "Ghoul",    160, 0.45f, 2.6f, 5.0f,    8.0f,  1,  0.0f,  1.3f, 1.0f,  0.8f, 0.9f,  SPAWN_RISE,  DEATH_FALL,      false },
    EnemyArchetype { "Bat",      90,  0.35f, 4.6f, 3.0f,    5.0f,  1,  1.2f,  1.5f, 1.2f,  0.7f, 0.7f,  SPAWN_SWOOP, DEATH_SPIN,      true  },
    EnemyArchetype { "Skeleton", 50,  0.4f,  2.4f, 7.0f,    6.0f,  2,  0.0f,  1.5f, 1.0f,  0.8f, 1.1f,  SPAWN_RISE,  DEATH_SHATTER,   false },
    EnemyArchetype { "Bloater",  30,  0.7f,  1.7f, 14.0f,   10.0f, 2,  0.0f,  1.5f, 0.5f,  0.8f, 0.18f, SPAWN_RISE,  DEATH_POP,       false },
    EnemyArchetype { "Wraith",   40,  0.4f,  3.9f, 9.0f,    9.0f,  2,  0.25f, 1.9f, 0.8f,  0.6f, 0.7f,  SPAWN_FADE,  DEATH_DISSIPATE, true  },
    EnemyArchetype { "Brute",    14,  0.95f, 1.8f, 70.0f,   16.0f, 5,  0.0f,  2.6f, 0.15f, 1.0f, 1.2f,  SPAWN_RISE,  DEATH_FALL,      false },
    EnemyArchetype { "Lich",     1,   1.2f,  1.6f, 2600.0f, 20.0f, 25, 0.35f, 3.6f, 0.0f,  2.0f, 2.4f,  SPAWN_RISE,  DEATH_BOSS,      true  },
};

inline constexpr StaticArray<const char*, ENEMY_TYPE_COUNT> cDeathSounds =
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


inline const StaticArray<UpgradeInfo, UPGRADE_COUNT + 1> cUpgrades =
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


inline constexpr StaticArray<const char*, UPGRADE_COUNT + 1> cUpgradeIcons =
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


inline constexpr StaticArray<const char*, PICKUP_COUNT> cPickupModels = { "Potion", "Magnet", "Bomb", "Chest" };
inline const StaticArray<Vec3, PICKUP_COUNT> cPickupColors = { Vec3(1.0f, 0.3f, 0.3f), Vec3(0.4f, 0.8f, 1.0f), Vec3(1.0f, 0.6f, 0.2f), Vec3(1.0f, 0.82f, 0.3f) };


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


inline const StaticArray<GroundLayer, GROUND_LAYER_COUNT> cGroundLayers =
{
    GroundLayer { "Grass", "Grass", Vec3(1.0f) },
    GroundLayer { "Dirt",  "Dirt",  Vec3(1.0f) },
    GroundLayer { "Stone", "Stone", Vec3(1.0f) },
    GroundLayer { "Moss",  "Grass", Vec3(1.25f, 1.3f, 1.1f) },
};


inline constexpr StaticArray<int, PARTICLE_MESH_COUNT> cParticlePoolSizes = { 320, 160, 220 };
inline constexpr StaticArray<const char*, PARTICLE_MESH_COUNT> cParticleModels = { "Spark", "Chunk", "Puff" };


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

} // RK
