#pragma once

#include "../Engine/Raekor.h"
#include "../Engine/Profiler.h"
#include "../Engine/GLTF.h"
#include "../MeshNodes.h"

#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Collision/GroupFilterTable.h"
#include "Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Jolt/Physics/Vehicle/VehicleConstraint.h"
#include "Jolt/Physics/Vehicle/VehicleCollisionTester.h"
#include "Jolt/Physics/Vehicle/WheeledVehicleController.h"

namespace RK {

static constexpr const char* cRuntimeRootName = "CoastRally Runtime";
static constexpr const char* cModelFile = "Assets/CoastRally/CoastRally.glb";
static constexpr const char* cPaletteFile = "Assets/CoastRally/Palette.png";
static constexpr const char* cPaletteEmissiveFile = "Assets/CoastRally/PaletteEmissive.png";
static constexpr const char* cPaletteSurfaceFile = "Assets/CoastRally/PaletteSurface.png";
static constexpr const char* cSkyVisibilityFile = "Assets/CoastRally/SkyVisibility.png";
static constexpr const char* cGroundAlbedoFile = "Assets/CoastRally/GroundAlbedo.png";
static constexpr const char* cTerrainAlbedoFile = "Assets/CoastRally/TerrainAlbedo.png";
static constexpr const char* cTerrainNormalFile = "Assets/CoastRally/TerrainNormal.png";
static constexpr const char* cTerrainDetailFile = "Assets/CoastRally/TerrainDetail.png";
static constexpr const char* cTerrainDetailNormalFile = "Assets/CoastRally/TerrainDetailNormal.png";
static constexpr uint8_t cTerrainNormalSwizzle = 0b11'10'01'11;
static constexpr float cTerrainDetailTiling = 100.0f;
static constexpr const char* cTerrainAsset = "Terrain";
static constexpr const char* cHudFontFile = "Assets/Fonts/Bangers-Regular.ttf";
static constexpr const char* cLabelFontFile = "Assets/Fonts/Inter-Medium.ttf";

static constexpr const char* cEngineSound = "Assets/CoastRally/Audio/Engine.ogg";
static constexpr const char* cSkidSound = "Assets/CoastRally/Audio/Skid.ogg";
static constexpr const char* cWindSound = "Assets/CoastRally/Audio/Wind.ogg";
static constexpr const char* cAmbienceSound = "Assets/CoastRally/Audio/Ambience.ogg";
static constexpr const char* cMusicSound = "Assets/CoastRally/Audio/Music.ogg";
static constexpr float cMusicVolume = 0.35f;
static constexpr const char* cImpactSound = "Assets/CoastRally/Audio/Impact.ogg";
static constexpr const char* cLandSound = "Assets/CoastRally/Audio/Land.ogg";
static constexpr const char* cSplashSound = "Assets/CoastRally/Audio/Splash.ogg";
static constexpr const char* cResetSound = "Assets/CoastRally/Audio/Reset.ogg";
static constexpr const char* cConeSound = "Assets/CoastRally/Audio/Cone.ogg";
static constexpr const char* cBarrelSound = "Assets/CoastRally/Audio/Barrel.ogg";
static constexpr const char* cCrateSound = "Assets/CoastRally/Audio/Crate.ogg";
static constexpr const char* cHaySound = "Assets/CoastRally/Audio/Hay.ogg";
static constexpr const char* cSelectSound = "Assets/CoastRally/Audio/Select.ogg";

static constexpr StaticArray<const char*, 9> cSoundEffects =
{
    cImpactSound, cLandSound, cSplashSound, cResetSound, cConeSound, cBarrelSound, cCrateSound, cHaySound, cSelectSound
};

static constexpr Vec3  cCarHalfExtents = Vec3(0.86f, 0.3f, 2.0f);
static constexpr float cCarMass = 1250.0f;
static constexpr float cWheelRadius = 0.36f;
static constexpr float cWheelWidth = 0.26f;
static constexpr Vec3  cWheelOffset = Vec3(0.94f, -0.05f, 0.0f);
static constexpr float cFrontWheelZ = 1.3f;
static constexpr float cRearWheelZ = -1.28f;
static constexpr float cSuspensionMinLength = 0.12f;
static constexpr float cSuspensionMaxLength = 0.42f;
static constexpr float cSuspensionFrequency = 1.7f;
static constexpr float cSuspensionDamping = 0.55f;
static constexpr float cCenterOfMassHeight = -0.55f;
static constexpr float cMaxPitchRollAngle = glm::radians(60.0f);
static constexpr float cAntiRollStiffness = 9000.0f;
static constexpr float cMaxSteerAngle = glm::radians(36.0f);
static constexpr float cWheelBase = cFrontWheelZ - cRearWheelZ;
static constexpr float cFrontLateralGrip = 1.7f;
static constexpr float cRearLateralGrip = 1.55f;
static constexpr float cLongitudinalGrip = 1.5f;
static constexpr float cEngineTorque = 650.0f;
static constexpr float cEngineMaxRPM = 7000.0f;
static constexpr float cShiftUpRPM = 5800.0f;
static constexpr float cShiftDownRPM = 2800.0f;
static constexpr float cHandBrakeTorque = 5000.0f;

static constexpr float cSteerSpeed = 6.0f;
static constexpr float cSteerReturnSpeed = 9.0f;
static constexpr float cSteerFalloffSpeed = 45.0f;
static constexpr float cHighSpeedSteerScale = 0.55f;
static constexpr float cYawAssist = 5.0f;
static constexpr float cYawAssistStraight = 1.2f;
static constexpr float cYawAssistGrip = 20.0f;
static constexpr float cDriftAssistDelay = 0.6f;
static constexpr float cSelfRighting = 4.0f;
static constexpr float cSelfRightingTilt = 0.85f;
static constexpr float cFlippedUp = 0.55f;
static constexpr float cDownforce = 0.004f;
static constexpr float cBoostAcceleration = 6.0f;
static constexpr float cMaxBoostSpeed = 55.0f;
static constexpr float cAirControl = 1.6f;

static constexpr float cSpawnHeight = 0.9f;
static constexpr float cDrownHeight = -1.4f;
static constexpr float cFlipResetTime = 2.5f;
static constexpr float cImpactSpeed = 3.5f;
static constexpr float cLandSpeed = 4.0f;

static constexpr float cChaseFov = 62.0f;
static constexpr float cMaxFovBoost = 14.0f;
static constexpr float cCameraClearance = 0.4f;
static constexpr float cMouseOrbitSpeed = 0.004f;
static constexpr float cOrbitReturnDelay = 1.2f;
static constexpr Vec3  cHoodCameraOffset = Vec3(0.0f, 0.47f, -0.1f);

static constexpr float cEngineIdleRPM = 1000.0f;
static constexpr float cEngineSoundRPM = 3000.0f;

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -200.0f, 0.0f);

static constexpr float cBrakeLightIntensity = 60000.0f;
static constexpr Vec3  cGlassColor = Vec3(0.015f, 0.02f, 0.025f);
static constexpr float cGlassOpacity = 0.6f;

static constexpr float cDayLength = 360.0f;
static constexpr float cStartTimeOfDay = 0.35f;
static constexpr float cSkyInclination = 0.5f;
static constexpr float cSkyFadeHeight = 0.12f;
static constexpr float cMinSkyHeight = 0.04f;
static constexpr float cMoonLift = 0.4f;
static constexpr float cHorizonSkyBrightness = 0.35f;
static constexpr float cHorizonIlluminance = 0.3f;
static constexpr Vec3  cSunsetColor = Vec3(1.0f, 0.4f, 0.16f);
static constexpr Vec3  cDaySunColor = Vec3(1.0f, 0.95f, 0.88f);
static constexpr Vec3  cMoonColor = Vec3(0.08f, 0.11f, 0.22f);
static constexpr float cSunIlluminance = 110000.0f;
static constexpr float cMoonIlluminance = 12000.0f;

static constexpr float cFogStart = 200.0f;
static constexpr float cFogEnd = 950.0f;
static constexpr float cFogOpacity = 1.0f;

static constexpr float cSkyVisibilityHalfSize = 262.0f;
static constexpr float cSkyVisibilityHeightMin = -5.0f;
static constexpr float cSkyVisibilityHeightRange = 80.0f;
static constexpr float cSkyVisibilityStrength = 0.85f;
static constexpr Vec3  cGroundAlbedo = Vec3(1.0f);
static constexpr float cGroundBounce = 1.0f;

static constexpr Vec3  cHeadlightOffset = Vec3(0.66f, 0.09f, 1.95f);
static constexpr Vec3  cHeadlightColor = Vec3(1.0f, 0.9f, 0.72f);
static constexpr float cHeadlightIntensity = 6000000.0f;
static constexpr float cHeadlightRange = 80.0f;
static constexpr float cHeadlightInnerAngle = glm::radians(10.0f);
static constexpr float cHeadlightOuterAngle = glm::radians(28.0f);
static constexpr float cHeadlightPitch = glm::radians(2.5f);

static constexpr float cVolumeStep = 0.05f;
static constexpr float cMinSensitivityScale = 0.2f;
static constexpr float cMaxSensitivityScale = 3.0f;


enum ERallyPauseTab
{
    RALLY_TAB_GENERAL,
    RALLY_TAB_GRAPHICS,
    RALLY_TAB_COUNT
};

static constexpr StaticArray<const char*, RALLY_TAB_COUNT> cRallyTabNames = { "GENERAL", "GRAPHICS" };


enum ERallySlider
{
    RALLY_SLIDER_NONE = -1,
    RALLY_SLIDER_VOLUME,
    RALLY_SLIDER_SENSITIVITY,
    RALLY_SLIDER_COUNT
};

static constexpr StaticArray<const char*, RALLY_SLIDER_COUNT> cRallySliderNames = { "Master volume", "Mouse sensitivity" };


struct RallyGraphicsSetting
{
    const char* mCVar;
    const char* mLabel;
};

static constexpr StaticArray<RallyGraphicsSetting, 6> cRallyGraphicsSettings =
{
    RallyGraphicsSetting { "r_enable_taa",     "Temporal anti-aliasing" },
    RallyGraphicsSetting { "r_enable_ddgi",    "Global illumination" },
    RallyGraphicsSetting { "r_enable_gtao",    "Ambient occlusion" },
    RallyGraphicsSetting { "r_enable_sky_visibility", "Sky occlusion" },
    RallyGraphicsSetting { "r_gtao_half_res",  "Half resolution AO" },
    RallyGraphicsSetting { "r_enable_shadows", "Ray traced shadows" },
};


enum ECameraMode
{
    CAMERA_CHASE,
    CAMERA_FAR,
    CAMERA_HOOD,
    CAMERA_MODE_COUNT
};


struct ChaseCamera
{
    float mDistance;
    float mHeight;
    float mLookHeight;
};

static constexpr StaticArray<ChaseCamera, 2> cChaseCameras =
{
    ChaseCamera { 6.8f,  2.2f, 1.0f },
    ChaseCamera { 11.0f, 3.8f, 1.2f },
};


enum EWheel
{
    WHEEL_FRONT_LEFT,
    WHEEL_FRONT_RIGHT,
    WHEEL_REAR_LEFT,
    WHEEL_REAR_RIGHT,
    WHEEL_COUNT
};


enum EPropType
{
    PROP_CONE,
    PROP_BARREL,
    PROP_CRATE,
    PROP_HAY_BALE,
    PROP_TYPE_COUNT
};


enum EPropShape
{
    PROP_SHAPE_BOX,
    PROP_SHAPE_CYLINDER
};


struct PropArchetype
{
    const char* mName;
    EPropShape mShape;
    Vec3 mHalfExtents;
    float mMass;
    const char* mSound;
};

static const StaticArray<PropArchetype, PROP_TYPE_COUNT> cPropArchetypes =
{
    PropArchetype { "Cone",    PROP_SHAPE_CYLINDER, Vec3(0.2f, 0.35f, 0.2f),     4.0f,  cConeSound },
    PropArchetype { "Barrel",  PROP_SHAPE_CYLINDER, Vec3(0.3f, 0.45f, 0.3f),     25.0f, cBarrelSound },
    PropArchetype { "Crate",   PROP_SHAPE_BOX,      Vec3(0.47f),                 30.0f, cCrateSound },
    PropArchetype { "HayBale", PROP_SHAPE_BOX,      Vec3(0.55f, 0.25f, 0.275f),  35.0f, cHaySound },
};

static constexpr StaticArray<const char*, 3> cDecorationAssets = { "Bush", "Grass", "Flowers" };
static constexpr StaticArray<const char*, 3> cLevelAssets = { "Terrain", "Road", "Pad" };
static constexpr StaticArray<const char*, 3> cGroundPlacements = { "Ramp", "Kicker", "Tabletop" };


enum ECollisionSubGroup
{
    COLLISION_SUBGROUP_CAR,
    COLLISION_SUBGROUP_OBSTACLE,
    COLLISION_SUBGROUP_COUNT
};


class RallyWheelFilter final : public JPH::BodyFilter
{
public:
    bool ShouldCollide(const JPH::BodyID& inBodyID) const override
    {
        return inBodyID != mCarBody;
    }

    bool ShouldCollideLocked(const JPH::Body& inBody) const override
    {
        return !( mGodMode && *mGodMode && inBody.GetCollisionGroup().GetSubGroupID() == COLLISION_SUBGROUP_OBSTACLE );
    }

    JPH::BodyID mCarBody;
    const int* mGodMode = nullptr;
};

} // RK
