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

    uint32_t LoadTexture(const char* inFile, bool inSRGB);
    void LoadTextures();
    void ReleaseModels();
    void LoadModels();
    Model CreateModel(const String& inName, Entity inMaterial = Entity::Null);
    void CreateMaterials();
    void CreateLevel();
    void AddCollision(const Array<PartTemplate>& inParts, const Mat4x4& inTransform);
    void CreateLevelBody();
    void DestroyLevelBody();
    void CreateCamera();

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

private:
    bool m_Started = false;
    float m_Time = 0.0f;
    float m_DriveTime = 0.0f;
    std::mt19937 m_Random;

    uint32_t m_PaletteTexture = 0;
    uint32_t m_PaletteEmissiveTexture = 0;
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
    Entity m_WaterMaterial = Entity::Null;
    Entity m_BrakeMaterial = Entity::Null;

    JPH::TriangleList m_LevelTriangles;
    JPH::BodyID m_LevelBody;

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

    Entity m_EngineEntity = Entity::Null;
    Entity m_SkidEntity = Entity::Null;
    Entity m_WindEntity = Entity::Null;
    Entity m_AmbienceEntity = Entity::Null;
    float m_LastImpactTime = 0.0f;

    bool m_ShowControls = true;
    Array<Popup> m_Popups;

    int* m_AutoDrive = nullptr;

public:
    CoastRallyScript()
    {
        m_WheelNodes.fill(Node::Null);
    }
};

} // RK
