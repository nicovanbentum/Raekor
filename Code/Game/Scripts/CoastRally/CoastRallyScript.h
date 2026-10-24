#pragma once

#include "CoastRally.h"

namespace RK {

class CoastRallyScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(CoastRallyScript);

    using Node = MeshNodes::Node;

    struct PartTemplate
    {
        String mName;
        Vec3 mJoint = Vec3(0.0f);
        Mesh mMesh;
    };

    struct ModelPart
    {
        String mName;
        Node mNode = Node::Null;
    };

    struct Model
    {
        Node mRoot = Node::Null;
        Array<ModelPart> mParts;
    };

    struct Placement
    {
        String mAsset;
        Vec3 mPosition = Vec3(0.0f);
        Quat mRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 mScale = Vec3(1.0f);
    };

    struct Prop
    {
        Node mNode = Node::Null;
        int mType = PROP_CONE;
        JPH::BodyID mBody;
        Vec3 mSpawnPosition = Vec3(0.0f);
        Quat mSpawnRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        bool mAwake = false;
        bool mKnocked = false;
    };

    struct Waypoint
    {
        Vec3 mPosition = Vec3(0.0f);
        Vec3 mForward = Vec3(0.0f, 0.0f, 1.0f);
    };

    struct Popup
    {
        String mText;
        float mTime = 0.0f;
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
    void SetTransform(Entity inEntity, const Vec3& inPosition, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f));
    void SetTransform(Node inNode, const Vec3& inPosition, const Quat& inRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f), const Vec3& inScale = Vec3(1.0f));
    void Park(Node inNode);
    void SetMaterial(Node inNode, Entity inMaterial);
    static Node FindPart(const Model& inModel, StringView inName);
    static Quat YawRotation(float inYaw);
    static float GetYaw(const Vec3& inDirection);
    static float LerpAngle(float inFrom, float inTo, float inT);
    static Vec3 ToVec3(JPH::Vec3Arg inVector);
    static Quat ToQuat(JPH::QuatArg inQuat);
    static JPH::Vec3 ToJolt(const Vec3& inVector);
    static JPH::Quat ToJolt(const Quat& inQuat);
    JPH::PhysicsSystem* GetPhysicsSystem();
    JPH::BodyInterface& GetBodyInterface();

    void DestroyRuntimeEntities();
    void NewGame();

    static String ConvertTexture(const String& inFile);
    uint32_t LoadTexture(const char* inFile, bool inSRGB, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA);
    uint32_t LoadCarTexture(const String& inCachedFile, bool inSRGB, uint8_t inSwizzle, uint32_t inDefault);
    void LoadTextures();
    void ReleaseModels();
    void LoadModels();
    Model CreateModel(const String& inName, Entity inMaterial = Entity::Null);
    void CreateMaterials();
    void CreateLevel();
    void AddCollision(JPH::TriangleList& outTriangles, const Array<PartTemplate>& inParts, const Mat4x4& inTransform);
    JPH::BodyID CreateStaticBody(JPH::TriangleList& ioTriangles, int inSubGroup);
    void CreateLevelBodies();
    void DestroyLevelBodies();
    JPH::CollisionGroup GetCollisionGroup(int inSubGroup) const;
    void ApplyGodMode();
    void ToggleGodMode();
    void CreateCamera();

    void CreateCarModel();
    void CreateCar();
    void DestroyCar();
    void PlaceCar(const Vec3& inPosition, const Vec3& inForward);
    void ResetCar();
    int FindNearestWaypoint(const Vec3& inPosition) const;
    void UpdateCar(float inDeltaTime);
    void UpdateCarVisuals();
    float GetForwardSpeed() const;
    int GetGroundedWheels() const;
    float GetSkidAmount() const;
    JPH::WheeledVehicleController* GetController() const;

    void CreateProps();
    void DestroyProps();
    void ResetProps();
    void UpdateProps();

    void UpdateCamera(float inDeltaTime);
    void SetCameraMode(int inMode);
    float CastCameraRay(const Vec3& inFrom, const Vec3& inTo);

    void CreateSky();
    void RestoreSky();
    void UpdateSky(float inDeltaTime);
    void CreateNightLights();
    void UpdateNightLights(float inDeltaTime, float inNight);

    void CreateAudio();
    void PlaySfx(const char* inFile, float inVolume = 1.0f, float inPitch = 1.0f);
    void PlaySfxAt(const char* inFile, const Vec3& inPosition, float inVolume = 1.0f, float inPitch = 1.0f);
    void PlayEntitySound(Entity inEntity);
    void StopEntitySound(Entity inEntity);
    void UpdateSounds(float inDeltaTime);

    static constexpr Vec4 cInk = Vec4(0.05f, 0.07f, 0.12f, 1.0f);
    static constexpr Vec4 cCream = Vec4(0.97f, 0.95f, 0.9f, 1.0f);
    static constexpr Vec4 cSun = Vec4(1.0f, 0.74f, 0.2f, 1.0f);
    static constexpr Vec4 cSea = Vec4(0.25f, 0.78f, 0.9f, 1.0f);
    static constexpr Vec4 cCoral = Vec4(1.0f, 0.42f, 0.32f, 1.0f);

    static Vec4 Fade(const Vec4& inColor, float inAlpha);
    void AddPopup(const String& inText, const Vec4& inColor);
    void DrawText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign = UI_TEXT_ALIGN_LEFT);
    float DrawKey(Vec2 inPos, StringView inLabel, float inScale);
    void DrawHUD(float inDeltaTime);
    void DrawSpeedometer(const Vec2& inDisplay, float inScale, float inMargin);
    void DrawTripInfo(float inScale, float inMargin);
    void DrawControls(const Vec2& inDisplay, float inScale, float inMargin);
    void DrawTitle(const Vec2& inDisplay, float inScale);
    void DrawPopups(const Vec2& inDisplay, float inScale, float inDeltaTime);

    struct PauseLayout
    {
        float mScale = 1.0f;
        Vec2 mDisplay = Vec2(0.0f);
        Vec2 mPanelPos = Vec2(0.0f);
        Vec2 mPanelSize = Vec2(0.0f);
        StaticArray<Vec2, RALLY_TAB_COUNT> mTabPos = {};
        Vec2 mTabSize = Vec2(0.0f);
        Vec2 mSliderPos = Vec2(0.0f);
        Vec2 mSliderSize = Vec2(0.0f);
        Vec2 mSliderPadding = Vec2(0.0f);
        float mSliderStep = 0.0f;
        Vec2 mToggleSize = Vec2(0.0f);
        Vec2 mGodModePos = Vec2(0.0f);
        Vec2 mAutoDrivePos = Vec2(0.0f);
        Vec2 mSettingsPos = Vec2(0.0f);
        float mSettingStep = 0.0f;
        Vec2 mButtonSize = Vec2(0.0f);
        Vec2 mResumePos = Vec2(0.0f);
        Vec2 mQuitPos = Vec2(0.0f);

        Vec2 GetSliderPos(int inSlider) const { return mSliderPos + Vec2(0.0f, mSliderStep * float(inSlider)); }
        Vec2 GetSettingPos(int inSetting) const { return mSettingsPos + Vec2(0.0f, mSettingStep * float(inSetting)); }
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
    void ToggleAutoDrive();
    static int* GetGraphicsSetting(int inSetting);
    void ToggleGraphicsSetting(int inSetting);
    void SetPauseTab(int inTab);
    void Resume();
    void Quit();
    void DrawPauseMenu();
    void DrawToggle(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inChecked, float inAlpha, float inScale);
    void DrawMenuButton(const Vec2& inLayoutPos, const Vec2& inSize, const Vec2& inOffset, StringView inLabel, bool inPrimary, float inAlpha, float inScale);
    void DrawPauseGeneralTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha);
    void DrawPauseGraphicsTab(const PauseLayout& inLayout, const Vec2& inOffset, float inAlpha);

private:
    bool m_Started = false;
    float m_Time = 0.0f;
    float m_DriveTime = 0.0f;
    std::mt19937 m_Random;

    uint32_t m_PaletteTexture = 0;
    uint32_t m_PaletteEmissiveTexture = 0;
    uint32_t m_PaletteRoughnessTexture = 0;
    uint32_t m_PaletteMetallicTexture = 0;
    uint32_t m_SkyVisibilityTexture = 0;
    uint32_t m_GroundAlbedoTexture = 0;
    uint32_t m_TerrainAlbedoTexture = 0;
    uint32_t m_TerrainNormalTexture = 0;
    uint32_t m_TerrainDetailTexture = 0;
    uint32_t m_TerrainDetailNormalTexture = 0;
    uint32_t m_BeamGradientTexture = 0;
    HashMap<String, uint32_t> m_CarTextures;
    int m_HudFont = 0;
    int m_LabelFont = 0;

    HashMap<String, Array<PartTemplate>> m_MeshTemplates;
    HashMap<String, Array<PartTemplate>> m_CollisionTemplates;
    Array<Placement> m_Placements;
    Array<Placement> m_PropPlacements;
    Array<Waypoint> m_Waypoints;
    Placement m_Spawn;

    MeshNodes m_Nodes;
    Entity m_RootEntity = Entity::Null;
    Entity m_CameraEntity = Entity::Null;

    Entity m_PaletteMaterial = Entity::Null;
    Entity m_TerrainMaterial = Entity::Null;
    Entity m_WaterMaterial = Entity::Null;
    Entity m_BrakeMaterial = Entity::Null;
    Entity m_TailLightMaterial = Entity::Null;
    Entity m_BeamMaterial = Entity::Null;

    JPH::TriangleList m_LevelTriangles;
    JPH::TriangleList m_ObstacleTriangles;
    JPH::BodyID m_LevelBody;
    JPH::BodyID m_ObstacleBody;
    JPH::Ref<JPH::GroupFilterTable> m_CollisionFilter;
    RallyWheelFilter m_WheelFilter;

    Model m_CarModel;
    Node m_BrakeNode = Node::Null;
    StaticArray<Node, WHEEL_COUNT> m_WheelNodes;
    JPH::BodyID m_CarBody;
    JPH::Ref<JPH::VehicleConstraint> m_Vehicle;

    Array<Prop> m_Props;

    float m_Steer = 0.0f;
    float m_PreviousForward = 1.0f;
    bool m_Braking = false;
    bool m_Boosting = false;
    float m_Throttle = 0.0f;
    float m_FlipTimer = 0.0f;
    float m_DriftTimer = 0.0f;
    float m_AirTime = 0.0f;
    float m_BestAirTime = 0.0f;
    float m_TopSpeed = 0.0f;
    float m_Odometer = 0.0f;
    Vec3 m_PreviousVelocity = Vec3(0.0f);
    Vec3 m_CarPosition = Vec3(0.0f);
    Quat m_CarRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    Vec3 m_CarVelocity = Vec3(0.0f);
    int m_PropsKnocked = 0;

    int m_CameraMode = CAMERA_CHASE;
    Vec3 m_CameraPosition = Vec3(0.0f);
    float m_CameraYaw = 0.0f;
    float m_OrbitYaw = 0.0f;
    float m_OrbitPitch = 0.0f;
    float m_OrbitIdle = 0.0f;
    float m_Shake = 0.0f;
    float m_Fov = cChaseFov;

    Entity m_SunEntity = Entity::Null;
    Array<Entity> m_StreetLampEntities;
    StaticArray<Entity, 2> m_LighthouseLightEntities = { Entity::Null, Entity::Null };
    Node m_LighthouseBeamNode = Node::Null;
    Vec3 m_LighthouseLampPosition = Vec3(0.0f);
    float m_LighthouseYaw = 0.0f;
    bool m_RestoreSun = false;
    DirectionalLight m_SavedSun;
    Quat m_SavedSunRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    float m_TimeOfDay = cStartTimeOfDay;
    float* m_DayLength = nullptr;
    StaticArray<Entity, 2> m_HeadlightEntities = { Entity::Null, Entity::Null };

    Entity m_EngineEntity = Entity::Null;
    Entity m_SkidEntity = Entity::Null;
    Entity m_WindEntity = Entity::Null;
    Entity m_AmbienceEntity = Entity::Null;
    Entity m_MusicEntity = Entity::Null;
    float m_LastImpactTime = 0.0f;

    bool m_ShowControls = true;
    Array<Popup> m_Popups;

    float m_PauseTime = 0.0f;
    int m_PauseTab = RALLY_TAB_GENERAL;
    Vec2 m_MousePosition = Vec2(-1.0f);
    int m_DraggingSlider = RALLY_SLIDER_NONE;

    int* m_GodMode = nullptr;
    int* m_AutoDrive = nullptr;
    float* m_SensitivityScale = nullptr;

public:
    CoastRallyScript()
    {
        m_WheelNodes.fill(Node::Null);
    }
};

} // RK
