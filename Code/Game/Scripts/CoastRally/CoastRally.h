#pragma once

#include "../Engine/Raekor.h"
#include "../Engine/Profiler.h"
#include "../Engine/GLTF.h"
#include "../MeshNodes.h"

#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Jolt/Physics/Vehicle/VehicleConstraint.h"
#include "Jolt/Physics/Vehicle/VehicleCollisionTester.h"
#include "Jolt/Physics/Vehicle/WheeledVehicleController.h"

namespace RK {

static constexpr const char* cRuntimeRootName = "CoastRally Runtime";
static constexpr const char* cModelFile = "Assets/CoastRally/CoastRally.glb";
static constexpr const char* cPaletteFile = "Assets/CoastRally/Palette.png";
static constexpr const char* cPaletteEmissiveFile = "Assets/CoastRally/PaletteEmissive.png";
static constexpr const char* cHudFontFile = "Assets/Fonts/Bangers-Regular.ttf";
static constexpr const char* cLabelFontFile = "Assets/Fonts/Inter-Medium.ttf";

static constexpr const char* cEngineSound = "Assets/CoastRally/Audio/Engine.ogg";
static constexpr const char* cSkidSound = "Assets/CoastRally/Audio/Skid.ogg";
static constexpr const char* cWindSound = "Assets/CoastRally/Audio/Wind.ogg";
static constexpr const char* cAmbienceSound = "Assets/CoastRally/Audio/Ambience.ogg";
static constexpr const char* cImpactSound = "Assets/CoastRally/Audio/Impact.ogg";
static constexpr const char* cLandSound = "Assets/CoastRally/Audio/Land.ogg";
static constexpr const char* cSplashSound = "Assets/CoastRally/Audio/Splash.ogg";
static constexpr const char* cResetSound = "Assets/CoastRally/Audio/Reset.ogg";
static constexpr const char* cConeSound = "Assets/CoastRally/Audio/Cone.ogg";
static constexpr const char* cBarrelSound = "Assets/CoastRally/Audio/Barrel.ogg";
static constexpr const char* cCrateSound = "Assets/CoastRally/Audio/Crate.ogg";
static constexpr const char* cHaySound = "Assets/CoastRally/Audio/Hay.ogg";

static constexpr StaticArray<const char*, 8> cSoundEffects =
{
    cImpactSound, cLandSound, cSplashSound, cResetSound, cConeSound, cBarrelSound, cCrateSound, cHaySound
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
static constexpr float cAntiRollStiffness = 4000.0f;
static constexpr float cMaxSteerAngle = glm::radians(34.0f);
static constexpr float cEngineTorque = 650.0f;
static constexpr float cEngineMaxRPM = 7000.0f;
static constexpr float cShiftUpRPM = 5800.0f;
static constexpr float cShiftDownRPM = 2800.0f;
static constexpr float cHandBrakeTorque = 5000.0f;

static constexpr float cSteerSpeed = 3.2f;
static constexpr float cSteerReturnSpeed = 5.0f;
static constexpr float cSteerFalloffSpeed = 40.0f;
static constexpr float cHighSpeedSteerScale = 0.35f;
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
static constexpr Vec3  cHoodCameraOffset = Vec3(0.0f, 0.62f, 0.15f);

static constexpr float cEngineIdleRPM = 1000.0f;
static constexpr float cEngineSoundRPM = 3000.0f;

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -200.0f, 0.0f);

static constexpr float cBrakeLightIntensity = 60000.0f;


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

} // RK
