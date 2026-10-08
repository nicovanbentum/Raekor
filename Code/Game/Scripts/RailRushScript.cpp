#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"

namespace RK {

static constexpr int   cLaneCount = 3;
static constexpr float cLaneWidth = 2.6f;
static constexpr float cStartSpeed = 12.0f;
static constexpr float cMaxSpeed = 26.0f;
static constexpr float cAcceleration = 0.12f;
static constexpr float cGravity = 32.0f;
static constexpr float cJumpVelocity = 10.5f;
static constexpr float cSneakersJumpVelocity = 14.5f;
static constexpr float cSlamVelocity = -24.0f;
static constexpr float cSlideDuration = 0.65f;
static constexpr float cStepHeight = 0.5f;
static constexpr float cPlayerHeight = 1.8f;
static constexpr float cSlideHeight = 0.8f;
static constexpr float cPowerUpDuration = 10.0f;
static constexpr float cGuardDuration = 6.0f;
static constexpr float cCrashDuration = 1.2f;

static constexpr StaticArray<float, 4> cCheatSpeeds = { 0.0f, 30.0f, 45.0f, 60.0f };

static constexpr float cTrainWidth = 2.3f;
static constexpr float cTrainHeight = 3.0f;
static constexpr float cRampLength = 9.0f;
static constexpr float cBarrierDepth = 0.3f;
static constexpr float cLowBarrierHeight = 1.0f;
static constexpr float cHighBarrierBottom = 1.35f;
static constexpr float cHighBarrierTop = 2.25f;
static constexpr float cOncomingSpeed = 9.0f;
static constexpr float cActivationDistance = 80.0f;

static constexpr float cSegmentLength = 30.0f;
static constexpr int   cSegmentCount = 9;
static constexpr int   cTiesPerSegment = 20;
static constexpr int   cBuildingsPerSide = 2;
static constexpr int   cWindowBandsPerBuilding = 4;
static constexpr float cGenerateDistance = 220.0f;
static constexpr float cRecycleDistance = 15.0f;

static constexpr int cTrainPoolSize = 18;
static constexpr int cRampPoolSize = 8;
static constexpr int cBarrierPoolSize = 14;
static constexpr int cCoinPoolSize = 180;
static constexpr int cTrainColorCount = 4;
static constexpr int cBuildingColorCount = 5;

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -60.0f, 0.0f);

static constexpr const char* cRuntimeRootName = "RailRush Runtime";


enum EObstacleType
{
    OBSTACLE_TRAIN,
    OBSTACLE_RAMP,
    OBSTACLE_LOW_BARRIER,
    OBSTACLE_HIGH_BARRIER,
    OBSTACLE_TYPE_COUNT
};


enum EPowerUp
{
    POWER_UP_MAGNET,
    POWER_UP_SNEAKERS,
    POWER_UP_MULTIPLIER,
    POWER_UP_COUNT
};


struct PowerUpInfo
{
    const char* mName;
    const char* mLabel;
    Vec3 mColor;
};


static const StaticArray<PowerUpInfo, POWER_UP_COUNT> cPowerUps =
{
    PowerUpInfo { "MAGNET",         "M",  Vec3(1.0f, 0.25f, 0.25f) },
    PowerUpInfo { "SUPER SNEAKERS", "S",  Vec3(0.3f, 1.0f, 0.45f) },
    PowerUpInfo { "2X SCORE",       "2X", Vec3(1.0f, 0.8f, 0.2f) },
};


class RailRushScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(RailRushScript);

    enum EState
    {
        STATE_READY,
        STATE_RUNNING,
        STATE_CRASHED,
        STATE_GAME_OVER
    };

    struct Obstacle
    {
        Entity mEntity = Entity::Null;
        StaticArray<Entity, 5> mParts;
        int mType = OBSTACLE_TRAIN;
        bool mActive = false;
        bool mMoving = false;
        int mLane = 0;
        float mZ = 0.0f;
        float mLength = 0.0f;
    };

    struct Coin
    {
        Entity mEntity = Entity::Null;
        bool mActive = false;
        bool mAttracted = false;
        Vec3 mPosition = Vec3(0.0f);
    };

    struct PowerUpPickup
    {
        Entity mEntity = Entity::Null;
        bool mActive = false;
        Vec3 mPosition = Vec3(0.0f);
    };

    struct Segment
    {
        float mZ = 0.0f;
        Entity mRoot = Entity::Null;
        StaticArray<Entity, cBuildingsPerSide * 2> mBuildings;
        StaticArray<Entity, cBuildingsPerSide * 2 * cWindowBandsPerBuilding> mWindows;
    };

    struct Popup
    {
        String mText;
        float mTime = 0.0f;
        Vec4 mColor = Vec4(1.0f);
    };

    void OnStart() override
    {
        DestroyRuntimeEntities();

        m_Random.seed(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

        m_RootEntity = m_Scene->CreateSpatialEntity(cRuntimeRootName);

        m_GodMode = &g_CVariables->Create("rr_god", 0, true);
        m_AutoStart = &g_CVariables->Create("rr_autostart", 0, true);
        m_SpeedCheat = &g_CVariables->Create("rr_speed", 0.0f, true);

        CreateMaterials();
        CreateSegments();
        CreatePlayer();
        CreatePools();
        CreateCamera();

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
        if (!m_Started)
            OnStart();

        inDeltaTime = glm::min(inDeltaTime, 0.05f);

        m_Time += inDeltaTime;

        if (m_State == STATE_READY && *m_AutoStart)
            m_State = STATE_RUNNING;

        if (m_State == STATE_RUNNING)
        {
            m_RunTime += inDeltaTime;

            UpdatePlayer(inDeltaTime);
            UpdateObstacles(inDeltaTime);
            UpdatePickups(inDeltaTime);
            UpdateGeneration();
        }
        else if (m_State == STATE_CRASHED && m_Time - m_CrashTime > cCrashDuration)
        {
            m_State = STATE_GAME_OVER;
        }

        UpdateSegments();
        UpdateVisuals(inDeltaTime);
        UpdateCamera(inDeltaTime);
        DrawHUD(inDeltaTime);
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (!m_Started || inEvent.type != SDL_EVENT_KEY_DOWN || inEvent.key.repeat)
            return;

        const SDL_Keycode key = inEvent.key.key;

        if (key == SDLK_F1)
        {
            *m_GodMode = !*m_GodMode;
            AddPopup(*m_GodMode ? "GOD MODE ON" : "GOD MODE OFF", Vec4(0.6f, 0.9f, 1.0f, 1.0f));
            return;
        }

        if (key == SDLK_F2)
        {
            const auto current = std::find(cCheatSpeeds.begin(), cCheatSpeeds.end(), *m_SpeedCheat);
            const size_t next = current == cCheatSpeeds.end() ? 1 : ( size_t(current - cCheatSpeeds.begin()) + 1 ) % cCheatSpeeds.size();

            *m_SpeedCheat = cCheatSpeeds[next];
            AddPopup(*m_SpeedCheat > 0.0f ? std::format("SPEED {} M/S", int(*m_SpeedCheat)) : String("NORMAL SPEED"), Vec4(0.6f, 0.9f, 1.0f, 1.0f));
            return;
        }

        if (m_State == STATE_READY)
        {
            if (key == SDLK_SPACE || key == SDLK_RETURN || key == SDLK_W || key == SDLK_UP)
            {
                m_State = STATE_RUNNING;
                AddPopup("GO!", Vec4(1.0f, 0.85f, 0.3f, 1.0f));
            }

            return;
        }

        if (m_State == STATE_GAME_OVER)
        {
            if (key == SDLK_SPACE || key == SDLK_RETURN)
                NewGame();

            return;
        }

        if (m_State != STATE_RUNNING)
            return;

        switch (key)
        {
            case SDLK_A:
            case SDLK_LEFT:  ChangeLane(-1); break;
            case SDLK_D:
            case SDLK_RIGHT: ChangeLane(1);  break;
            case SDLK_W:
            case SDLK_UP:
            case SDLK_SPACE: Jump();         break;
            case SDLK_S:
            case SDLK_DOWN:  Slide();        break;
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

    Entity CreateShape(StringView inName, bool inIsSphere, Entity inMaterial, Vec3 inPosition, Vec3 inScale, Entity inParent = Entity::Null, Quat inRotation = Quat(Vec3(0.0f)))
    {
        const Entity entity = CreateEntity(inName, inParent);

        Transform& transform = m_Scene->Get<Transform>(entity);
        transform.position = inPosition;
        transform.rotation = inRotation;
        transform.scale = inScale;
        transform.Compose();

        Mesh& mesh = m_Scene->Add<Mesh>(entity);

        if (inIsSphere)
            Mesh::CreateSphere(mesh, 0.5f, 16, 12);
        else
            Mesh::CreateCube(mesh, 1.0f);

        mesh.material = inMaterial;

        m_App->GetRenderInterface()->UploadMeshBuffers(entity, mesh);

        return entity;
    }

    void SetTransform(Entity inEntity, const Vec3& inPosition, const Vec3& inScale = Vec3(1.0f), const Quat& inRotation = Quat(Vec3(0.0f)))
    {
        if (Transform* transform = FindComponent<Transform>(inEntity))
        {
            transform->position = inPosition;
            transform->rotation = inRotation;
            transform->scale = inScale;
            transform->Compose();
        }
    }

    void Park(Entity inEntity)
    {
        SetTransform(inEntity, cParkedPosition, Vec3(0.1f));
    }

    void SetMaterial(Entity inEntity, Entity inMaterial)
    {
        if (Mesh* mesh = FindComponent<Mesh>(inEntity))
            mesh->material = inMaterial;
    }

    float RandomFloat(float inMin, float inMax)
    {
        return std::uniform_real_distribution<float>(inMin, inMax)(m_Random);
    }

    int RandomInt(int inMin, int inMax)
    {
        return std::uniform_int_distribution<int>(inMin, inMax)(m_Random);
    }

    static float GetLaneX(int inLane)
    {
        return float(1 - inLane) * cLaneWidth;
    }

    static int GetNearestLane(float inX)
    {
        return glm::clamp(int(glm::round(1.0f - inX / cLaneWidth)), 0, cLaneCount - 1);
    }

    void CreateMaterials()
    {
        static constexpr StaticArray<Vec3, cTrainColorCount> cTrainColors =
        {
            Vec3(0.15f, 0.35f, 0.75f), Vec3(0.75f, 0.15f, 0.12f), Vec3(0.15f, 0.55f, 0.30f), Vec3(0.70f, 0.72f, 0.75f)
        };

        static constexpr StaticArray<Vec3, cBuildingColorCount> cBuildingColors =
        {
            Vec3(0.55f, 0.28f, 0.20f), Vec3(0.78f, 0.70f, 0.55f), Vec3(0.30f, 0.50f, 0.52f), Vec3(0.62f, 0.60f, 0.62f), Vec3(0.80f, 0.55f, 0.35f)
        };

        for (int index = 0; index < cTrainColorCount; index++)
            m_TrainMaterials[index] = CreateMaterial(cTrainColors[index], Vec3(0.0f), 0.35f, index == cTrainColorCount - 1 ? 0.8f : 0.2f);

        for (int index = 0; index < cBuildingColorCount; index++)
            m_BuildingMaterials[index] = CreateMaterial(cBuildingColors[index], Vec3(0.0f), 0.85f);

        m_GravelMaterial = CreateMaterial(Vec3(0.32f, 0.30f, 0.28f), Vec3(0.0f), 0.95f);
        m_ConcreteMaterial = CreateMaterial(Vec3(0.55f, 0.55f, 0.53f), Vec3(0.0f), 0.9f);
        m_TieMaterial = CreateMaterial(Vec3(0.25f, 0.17f, 0.11f), Vec3(0.0f), 0.9f);
        m_RailMaterial = CreateMaterial(Vec3(0.75f, 0.75f, 0.78f), Vec3(0.0f), 0.25f, 1.0f);
        m_WindowMaterial = CreateMaterial(Vec3(0.95f, 0.9f, 0.7f), Vec3(1.0f, 0.9f, 0.65f) * 2500.0f, 0.2f);
        m_GlassMaterial = CreateMaterial(Vec3(0.12f, 0.18f, 0.25f), Vec3(0.0f), 0.08f, 0.9f);
        m_LampMaterial = CreateMaterial(Vec3(1.0f, 0.95f, 0.8f), Vec3(1.0f, 0.92f, 0.75f) * 20000.0f, 0.3f);
        m_PostMaterial = CreateMaterial(Vec3(0.12f, 0.13f, 0.15f), Vec3(0.0f), 0.5f, 0.8f);
        m_TrainWindowMaterial = CreateMaterial(Vec3(0.6f, 0.8f, 0.9f), Vec3(0.7f, 0.85f, 1.0f) * 1500.0f, 0.1f);
        m_HeadlightMaterial = CreateMaterial(Vec3(1.0f, 0.95f, 0.8f), Vec3(1.0f, 0.95f, 0.8f) * 60000.0f, 0.2f);
        m_DarkLightMaterial = CreateMaterial(Vec3(0.2f, 0.2f, 0.2f), Vec3(0.0f), 0.3f);
        m_RampMaterial = CreateMaterial(Vec3(0.45f, 0.42f, 0.38f), Vec3(0.0f), 0.6f, 0.6f);
        m_BarrierMaterial = CreateMaterial(Vec3(0.85f, 0.15f, 0.12f), Vec3(0.0f), 0.5f);
        m_StripeMaterial = CreateMaterial(Vec3(0.95f, 0.95f, 0.95f), Vec3(0.0f), 0.5f);
        m_HighBarrierMaterial = CreateMaterial(Vec3(0.95f, 0.75f, 0.1f), Vec3(0.0f), 0.5f);
        m_CoinMaterial = CreateMaterial(Vec3(1.0f, 0.78f, 0.2f), Vec3(1.0f, 0.7f, 0.15f) * 3000.0f, 0.2f, 1.0f);
        m_HoodieMaterial = CreateMaterial(Vec3(0.85f, 0.25f, 0.45f), Vec3(0.0f), 0.7f);
        m_PantsMaterial = CreateMaterial(Vec3(0.15f, 0.20f, 0.35f), Vec3(0.0f), 0.8f);
        m_SkinMaterial = CreateMaterial(Vec3(0.85f, 0.65f, 0.50f), Vec3(0.0f), 0.6f);
        m_CapMaterial = CreateMaterial(Vec3(0.15f, 0.55f, 0.95f), Vec3(0.0f), 0.6f);
        m_BackpackMaterial = CreateMaterial(Vec3(0.95f, 0.65f, 0.15f), Vec3(0.0f), 0.7f);
        m_SneakerMaterial = CreateMaterial(Vec3(0.95f, 0.95f, 0.95f), Vec3(0.0f), 0.5f);
        m_GlowingSneakerMaterial = CreateMaterial(Vec3(0.3f, 1.0f, 0.45f), Vec3(0.3f, 1.0f, 0.45f) * 8000.0f, 0.3f);

        for (int index = 0; index < POWER_UP_COUNT; index++)
            m_PowerUpMaterials[index] = CreateMaterial(cPowerUps[index].mColor, cPowerUps[index].mColor * 12000.0f, 0.2f);
    }

    void CreateSegments()
    {
        const float track_width = cLaneCount * cLaneWidth + 1.0f;

        for (Segment& segment : m_Segments)
        {
            segment.mRoot = CreateEntity("Track Segment");

            const Vec3 half_offset = Vec3(0.0f, 0.0f, cSegmentLength * 0.5f);

            CreateShape("Gravel", false, m_GravelMaterial, Vec3(0.0f, -0.25f, 0.0f) + half_offset, Vec3(track_width, 0.5f, cSegmentLength), segment.mRoot);

            for (const float side : { -1.0f, 1.0f })
            {
                CreateShape("Platform", false, m_ConcreteMaterial, Vec3(side * ( track_width * 0.5f + 2.0f ), 0.3f, 0.0f) + half_offset, Vec3(4.0f, 1.1f, cSegmentLength), segment.mRoot);
                CreateShape("Lamp Post", false, m_PostMaterial, Vec3(side * ( track_width * 0.5f + 1.2f ), 3.0f, 4.0f), Vec3(0.18f, 5.4f, 0.18f), segment.mRoot);
                CreateShape("Lamp Arm", false, m_PostMaterial, Vec3(side * ( track_width * 0.5f + 0.6f ), 5.6f, 4.0f), Vec3(1.2f, 0.12f, 0.12f), segment.mRoot);
                CreateShape("Lamp", false, m_LampMaterial, Vec3(side * ( track_width * 0.5f + 0.1f ), 5.45f, 4.0f), Vec3(0.45f, 0.15f, 0.3f), segment.mRoot);
            }

            for (int tie = 0; tie < cTiesPerSegment; tie++)
            {
                const float z = ( float(tie) + 0.5f ) * cSegmentLength / float(cTiesPerSegment);
                CreateShape("Tie", false, m_TieMaterial, Vec3(0.0f, 0.03f, z), Vec3(track_width - 0.6f, 0.1f, 0.35f), segment.mRoot);
            }

            for (int lane = 0; lane < cLaneCount; lane++)
            {
                for (const float side : { -1.0f, 1.0f })
                    CreateShape("Rail", false, m_RailMaterial, Vec3(GetLaneX(lane) + side * 0.72f, 0.14f, 0.0f) + half_offset, Vec3(0.1f, 0.14f, cSegmentLength), segment.mRoot);
            }

            for (Entity& building : segment.mBuildings)
                building = CreateShape("Building", false, m_BuildingMaterials[0], Vec3(0.0f), Vec3(1.0f), segment.mRoot);

            for (Entity& window : segment.mWindows)
                window = CreateShape("Windows", false, m_WindowMaterial, Vec3(0.0f), Vec3(1.0f), segment.mRoot);
        }
    }

    void RandomizeSegment(Segment& inSegment)
    {
        const float track_width = cLaneCount * cLaneWidth + 1.0f;
        const float building_length = cSegmentLength / cBuildingsPerSide;

        for (int index = 0; index < cBuildingsPerSide * 2; index++)
        {
            const float side = index < cBuildingsPerSide ? -1.0f : 1.0f;
            const float height = RandomFloat(7.0f, 22.0f);
            const float depth = RandomFloat(5.0f, 8.0f);
            const float inner_x = side * ( track_width * 0.5f + 4.2f );
            const float z = ( float(index % cBuildingsPerSide) + 0.5f ) * building_length;

            SetMaterial(inSegment.mBuildings[index], m_BuildingMaterials[RandomInt(0, cBuildingColorCount - 1)]);
            SetTransform(inSegment.mBuildings[index], Vec3(inner_x + side * depth * 0.5f, height * 0.5f, z), Vec3(depth, height, building_length - 0.6f));

            for (int band = 0; band < cWindowBandsPerBuilding; band++)
            {
                const Entity window = inSegment.mWindows[index * cWindowBandsPerBuilding + band];
                const float band_y = 3.0f + band * 4.0f;

                if (band_y + 1.5f > height)
                {
                    Park(window);
                    continue;
                }

                SetMaterial(window, RandomFloat(0.0f, 1.0f) < 0.6f ? m_WindowMaterial : m_GlassMaterial);
                SetTransform(window, Vec3(inner_x - side * 0.02f, band_y, z), Vec3(0.06f, 1.4f, building_length - 2.5f));
            }
        }

        SetTransform(inSegment.mRoot, Vec3(0.0f, 0.0f, inSegment.mZ));
    }

    void CreatePlayer()
    {
        m_PlayerEntity = CreateEntity("Runner");
        m_BodyPivot = CreateEntity("Body", m_PlayerEntity);

        CreateShape("Torso", false, m_HoodieMaterial, Vec3(0.0f, 1.12f, 0.0f), Vec3(0.56f, 0.66f, 0.34f), m_BodyPivot);
        CreateShape("Head", true, m_SkinMaterial, Vec3(0.0f, 1.66f, 0.0f), Vec3(0.42f), m_BodyPivot);
        CreateShape("Cap", false, m_CapMaterial, Vec3(0.0f, 1.83f, 0.04f), Vec3(0.44f, 0.12f, 0.46f), m_BodyPivot);
        CreateShape("Cap Brim", false, m_CapMaterial, Vec3(0.0f, 1.79f, 0.3f), Vec3(0.36f, 0.04f, 0.22f), m_BodyPivot);
        CreateShape("Backpack", false, m_BackpackMaterial, Vec3(0.0f, 1.15f, -0.27f), Vec3(0.44f, 0.5f, 0.22f), m_BodyPivot);

        for (int side = 0; side < 2; side++)
        {
            const float x = side == 0 ? 0.15f : -0.15f;

            m_LegPivots[side] = CreateEntity("Leg Pivot", m_BodyPivot);
            SetTransform(m_LegPivots[side], Vec3(x, 0.8f, 0.0f));
            CreateShape("Leg", false, m_PantsMaterial, Vec3(0.0f, -0.38f, 0.0f), Vec3(0.2f, 0.72f, 0.22f), m_LegPivots[side]);
            m_Sneakers[side] = CreateShape("Sneaker", false, m_SneakerMaterial, Vec3(0.0f, -0.75f, 0.06f), Vec3(0.22f, 0.12f, 0.34f), m_LegPivots[side]);

            const float arm_x = side == 0 ? 0.36f : -0.36f;

            m_ArmPivots[side] = CreateEntity("Arm Pivot", m_BodyPivot);
            SetTransform(m_ArmPivots[side], Vec3(arm_x, 1.38f, 0.0f));
            CreateShape("Arm", false, m_HoodieMaterial, Vec3(0.0f, -0.28f, 0.0f), Vec3(0.15f, 0.58f, 0.17f), m_ArmPivots[side]);
            CreateShape("Hand", true, m_SkinMaterial, Vec3(0.0f, -0.6f, 0.0f), Vec3(0.15f), m_ArmPivots[side]);
        }
    }

    Obstacle CreateObstacle(int inType)
    {
        Obstacle obstacle;
        obstacle.mType = inType;
        obstacle.mParts.fill(Entity::Null);

        switch (inType)
        {
            case OBSTACLE_TRAIN:
            {
                obstacle.mEntity = CreateEntity("Train");
                obstacle.mParts[0] = CreateShape("Train Body", false, m_TrainMaterials[0], Vec3(0.0f), Vec3(1.0f), obstacle.mEntity);
                obstacle.mParts[1] = CreateShape("Train Windows", false, m_TrainWindowMaterial, Vec3(0.0f), Vec3(1.0f), obstacle.mEntity);
                obstacle.mParts[2] = CreateShape("Train Roof", false, m_RailMaterial, Vec3(0.0f), Vec3(1.0f), obstacle.mEntity);
                obstacle.mParts[3] = CreateShape("Headlight", true, m_DarkLightMaterial, Vec3(0.7f, 0.9f, -0.02f), Vec3(0.4f, 0.4f, 0.1f), obstacle.mEntity);
                obstacle.mParts[4] = CreateShape("Headlight", true, m_DarkLightMaterial, Vec3(-0.7f, 0.9f, -0.02f), Vec3(0.4f, 0.4f, 0.1f), obstacle.mEntity);
            } break;

            case OBSTACLE_RAMP:
            {
                const float slope_length = glm::sqrt(cRampLength * cRampLength + cTrainHeight * cTrainHeight);
                const float angle = glm::atan(cTrainHeight, cRampLength);

                obstacle.mLength = cRampLength;
                obstacle.mEntity = CreateEntity("Ramp");
                obstacle.mParts[0] = CreateShape("Ramp Surface", false, m_RampMaterial, Vec3(0.0f, cTrainHeight * 0.5f - 0.15f, cRampLength * 0.5f), Vec3(cTrainWidth, 0.3f, slope_length), obstacle.mEntity, Quat(Vec3(-angle, 0.0f, 0.0f)));
                obstacle.mParts[1] = CreateShape("Ramp Support", false, m_PostMaterial, Vec3(0.0f, cTrainHeight * 0.5f, cRampLength - 0.3f), Vec3(cTrainWidth - 0.4f, cTrainHeight, 0.3f), obstacle.mEntity);
            } break;

            case OBSTACLE_LOW_BARRIER:
            {
                obstacle.mLength = cBarrierDepth;
                obstacle.mEntity = CreateEntity("Low Barrier");
                obstacle.mParts[0] = CreateShape("Barrier", false, m_BarrierMaterial, Vec3(0.0f, cLowBarrierHeight * 0.5f, cBarrierDepth * 0.5f), Vec3(cTrainWidth, cLowBarrierHeight, cBarrierDepth), obstacle.mEntity);
                obstacle.mParts[1] = CreateShape("Stripe", false, m_StripeMaterial, Vec3(0.0f, cLowBarrierHeight * 0.7f, -0.01f), Vec3(cTrainWidth, 0.18f, 0.02f), obstacle.mEntity);
            } break;

            case OBSTACLE_HIGH_BARRIER:
            {
                const float bar_height = cHighBarrierTop - cHighBarrierBottom;

                obstacle.mLength = cBarrierDepth;
                obstacle.mEntity = CreateEntity("High Barrier");
                obstacle.mParts[0] = CreateShape("Post", false, m_PostMaterial, Vec3(cTrainWidth * 0.5f, cHighBarrierTop * 0.5f, cBarrierDepth * 0.5f), Vec3(0.14f, cHighBarrierTop, 0.14f), obstacle.mEntity);
                obstacle.mParts[1] = CreateShape("Post", false, m_PostMaterial, Vec3(-cTrainWidth * 0.5f, cHighBarrierTop * 0.5f, cBarrierDepth * 0.5f), Vec3(0.14f, cHighBarrierTop, 0.14f), obstacle.mEntity);
                obstacle.mParts[2] = CreateShape("Bar", false, m_HighBarrierMaterial, Vec3(0.0f, cHighBarrierBottom + bar_height * 0.5f, cBarrierDepth * 0.5f), Vec3(cTrainWidth, bar_height, 0.16f), obstacle.mEntity);
                obstacle.mParts[3] = CreateShape("Stripe", false, m_PostMaterial, Vec3(0.0f, cHighBarrierBottom + bar_height * 0.5f, -0.01f), Vec3(cTrainWidth, bar_height * 0.3f, 0.02f), obstacle.mEntity);
            } break;
        }

        Park(obstacle.mEntity);
        return obstacle;
    }

    void CreatePools()
    {
        m_Obstacles.clear();

        for (int index = 0; index < cTrainPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_TRAIN));

        for (int index = 0; index < cRampPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_RAMP));

        for (int index = 0; index < cBarrierPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_LOW_BARRIER));

        for (int index = 0; index < cBarrierPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_HIGH_BARRIER));

        m_Coins.resize(cCoinPoolSize);
        for (Coin& coin : m_Coins)
            coin.mEntity = CreateShape("Coin", true, m_CoinMaterial, cParkedPosition, Vec3(0.1f));

        for (int index = 0; index < POWER_UP_COUNT; index++)
            m_PowerUpPickups[index].mEntity = CreateShape(cPowerUps[index].mName, true, m_PowerUpMaterials[index], cParkedPosition, Vec3(0.1f));
    }

    void CreateCamera()
    {
        m_CameraEntity = CreateEntity("Runner Camera");

        Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
        camera.SetFov(62.0f);

        m_App->SetCameraEntity(m_CameraEntity);
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
        m_BodyPivot = Entity::Null;
        m_LegPivots.fill(Entity::Null);
        m_ArmPivots.fill(Entity::Null);
        m_Sneakers.fill(Entity::Null);
        m_Obstacles.clear();
        m_Coins.clear();

        for (Segment& segment : m_Segments)
            segment = Segment();

        for (PowerUpPickup& pickup : m_PowerUpPickups)
            pickup = PowerUpPickup();
    }

    void NewGame()
    {
        for (Obstacle& obstacle : m_Obstacles)
        {
            obstacle.mActive = false;
            Park(obstacle.mEntity);
        }

        for (Coin& coin : m_Coins)
        {
            coin.mActive = false;
            Park(coin.mEntity);
        }

        for (PowerUpPickup& pickup : m_PowerUpPickups)
        {
            pickup.mActive = false;
            Park(pickup.mEntity);
        }

        for (int index = 0; index < cSegmentCount; index++)
        {
            m_Segments[index].mZ = ( float(index) - 1.0f ) * cSegmentLength;
            RandomizeSegment(m_Segments[index]);
        }

        m_State = STATE_READY;
        m_RunTime = 0.0f;
        m_Speed = cStartSpeed;
        m_PlayerZ = 0.0f;
        m_PlayerX = GetLaneX(1);
        m_PlayerY = 0.0f;
        m_VelocityY = 0.0f;
        m_Lane = 1;
        m_PreviousLane = 1;
        m_Grounded = true;
        m_SlideTimer = 0.0f;
        m_BounceTimer = 0.0f;
        m_GuardTimer = 0.0f;
        m_CrashTime = 0.0f;
        m_RunPhase = 0.0f;
        m_Score = 0.0f;
        m_CoinCount = 0;
        m_PowerUpTimers.fill(0.0f);
        m_SafeLane = 1;
        m_NextRowZ = 40.0f;
        m_LaneBlockedUntil.fill(0.0f);
        m_DistanceSincePowerUp = 0.0f;
        m_Popups.clear();
        m_CameraPosition = Vec3(GetLaneX(1), 3.2f, -6.5f);
        m_CameraLookY = 1.2f;
        m_Shake = 0.0f;

        UpdateGeneration();
    }

    float GetSurfaceHeight(int inLane, float inZ) const
    {
        float height = 0.0f;

        for (const Obstacle& obstacle : m_Obstacles)
        {
            if (!obstacle.mActive || obstacle.mLane != inLane)
                continue;

            if (inZ < obstacle.mZ || inZ > obstacle.mZ + obstacle.mLength)
                continue;

            if (obstacle.mType == OBSTACLE_TRAIN)
                height = glm::max(height, cTrainHeight);
            else if (obstacle.mType == OBSTACLE_RAMP)
                height = glm::max(height, cTrainHeight * ( inZ - obstacle.mZ ) / obstacle.mLength);
        }

        return height;
    }

    void ChangeLane(int inDirection)
    {
        const int target = m_Lane + inDirection;

        if (target < 0 || target >= cLaneCount)
            return;

        const bool blocked = GetSurfaceHeight(target, m_PlayerZ) > m_PlayerY + cStepHeight || GetSurfaceHeight(target, m_PlayerZ + 1.0f) > m_PlayerY + cStepHeight;

        if (blocked && !*m_GodMode)
        {
            Stumble(inDirection);
            return;
        }

        m_PreviousLane = m_Lane;
        m_Lane = target;
    }

    void Stumble(int inDirection)
    {
        m_BounceTimer = 0.25f;
        m_BounceDirection = float(inDirection);
        m_Shake = glm::max(m_Shake, 0.3f);

        if (m_GuardTimer > 0.0f)
        {
            Crash("CAUGHT BY THE GUARD");
            return;
        }

        m_GuardTimer = cGuardDuration;
        AddPopup("WATCH OUT!", Vec4(1.0f, 0.4f, 0.3f, 1.0f));
    }

    void Jump()
    {
        if (!m_Grounded)
            return;

        m_VelocityY = m_PowerUpTimers[POWER_UP_SNEAKERS] > 0.0f ? cSneakersJumpVelocity : cJumpVelocity;
        m_Grounded = false;
        m_SlideTimer = 0.0f;
    }

    void Slide()
    {
        m_SlideTimer = cSlideDuration;

        if (!m_Grounded)
            m_VelocityY = glm::min(m_VelocityY, cSlamVelocity);
    }

    void Crash(const char* inReason)
    {
        m_State = STATE_CRASHED;
        m_CrashTime = m_Time;
        m_CrashReason = inReason;
        m_Shake = 0.8f;
        m_BestScore = glm::max(m_BestScore, int(m_Score));
    }

    void UpdatePlayer(float inDeltaTime)
    {
        m_Speed = *m_SpeedCheat > 0.0f ? *m_SpeedCheat : glm::min(cMaxSpeed, cStartSpeed + m_RunTime * cAcceleration);

        const float previous_z = m_PlayerZ;
        m_PlayerZ += m_Speed * inDeltaTime;

        const float multiplier = m_PowerUpTimers[POWER_UP_MULTIPLIER] > 0.0f ? 2.0f : 1.0f;
        m_Score += m_Speed * inDeltaTime * multiplier;
        m_DistanceSincePowerUp += m_Speed * inDeltaTime;

        m_PlayerX = glm::mix(m_PlayerX, GetLaneX(m_Lane), 1.0f - glm::exp(-inDeltaTime * 16.0f));

        m_BounceTimer = glm::max(0.0f, m_BounceTimer - inDeltaTime);
        m_GuardTimer = glm::max(0.0f, m_GuardTimer - inDeltaTime);
        m_SlideTimer = glm::max(0.0f, m_SlideTimer - ( m_Grounded ? inDeltaTime : 0.0f ));

        for (float& timer : m_PowerUpTimers)
            timer = glm::max(0.0f, timer - inDeltaTime);

        const float previous_y = m_PlayerY;

        m_VelocityY -= cGravity * inDeltaTime;
        m_PlayerY += m_VelocityY * inDeltaTime;

        const int lane = GetNearestLane(m_PlayerX);
        float ground = GetSurfaceHeight(lane, m_PlayerZ);

        if (ground > previous_y + cStepHeight && *m_GodMode)
            ground = glm::min(ground, previous_y);

        if (ground > previous_y + cStepHeight)
        {
            Crash("HIT A TRAIN");
            m_PlayerZ = previous_z;
            m_PlayerY = previous_y;
            return;
        }

        if (m_PlayerY <= ground)
        {
            m_PlayerY = ground;
            m_VelocityY = 0.0f;
            m_Grounded = true;
        }
        else
        {
            m_Grounded = m_PlayerY - ground < 0.05f && m_VelocityY <= 0.0f;
        }

        const bool sliding = m_SlideTimer > 0.0f;

        for (const Obstacle& obstacle : m_Obstacles)
        {
            if (!obstacle.mActive || obstacle.mLane != lane)
                continue;

            if (obstacle.mType != OBSTACLE_LOW_BARRIER && obstacle.mType != OBSTACLE_HIGH_BARRIER)
                continue;

            if (*m_GodMode)
                continue;

            if (obstacle.mZ + obstacle.mLength < previous_z - 0.3f || obstacle.mZ > m_PlayerZ + 0.3f)
                continue;

            const float top = m_PlayerY + ( sliding ? cSlideHeight : cPlayerHeight );

            if (obstacle.mType == OBSTACLE_LOW_BARRIER && m_PlayerY < cLowBarrierHeight)
            {
                Crash("HIT A BARRIER");
                return;
            }

            if (obstacle.mType == OBSTACLE_HIGH_BARRIER && m_PlayerY < cHighBarrierTop && top > cHighBarrierBottom)
            {
                Crash("HIT A BARRIER");
                return;
            }
        }
    }

    void UpdateObstacles(float inDeltaTime)
    {
        for (Obstacle& obstacle : m_Obstacles)
        {
            if (!obstacle.mActive)
                continue;

            if (obstacle.mMoving && obstacle.mZ - m_PlayerZ < cActivationDistance)
                obstacle.mZ -= cOncomingSpeed * inDeltaTime;

            if (obstacle.mZ + obstacle.mLength < m_PlayerZ - cRecycleDistance)
            {
                obstacle.mActive = false;
                Park(obstacle.mEntity);
            }
        }
    }

    void UpdatePickups(float inDeltaTime)
    {
        const Vec3 player_center = Vec3(m_PlayerX, m_PlayerY + ( m_SlideTimer > 0.0f ? 0.4f : 0.9f ), m_PlayerZ);
        const bool magnet = m_PowerUpTimers[POWER_UP_MAGNET] > 0.0f;
        const float multiplier = m_PowerUpTimers[POWER_UP_MULTIPLIER] > 0.0f ? 2.0f : 1.0f;

        for (Coin& coin : m_Coins)
        {
            if (!coin.mActive)
                continue;

            if (magnet && !coin.mAttracted && coin.mPosition.z - m_PlayerZ < 14.0f && coin.mPosition.z > m_PlayerZ - 1.0f)
                coin.mAttracted = true;

            if (coin.mAttracted)
            {
                const Vec3 delta = player_center - coin.mPosition;
                const float distance = glm::length(delta);
                coin.mPosition += delta / glm::max(distance, 0.001f) * glm::min(distance, ( m_Speed + 20.0f ) * inDeltaTime);
                coin.mPosition.z += m_Speed * inDeltaTime;
            }

            const Vec3 delta = glm::abs(coin.mPosition - player_center);

            if (delta.x < 0.9f && delta.y < 1.1f && delta.z < 0.9f)
            {
                coin.mActive = false;
                Park(coin.mEntity);
                m_CoinCount++;
                m_Score += 10.0f * multiplier;
            }
            else if (coin.mPosition.z < m_PlayerZ - cRecycleDistance)
            {
                coin.mActive = false;
                Park(coin.mEntity);
            }
        }

        for (int index = 0; index < POWER_UP_COUNT; index++)
        {
            PowerUpPickup& pickup = m_PowerUpPickups[index];

            if (!pickup.mActive)
                continue;

            const Vec3 delta = glm::abs(pickup.mPosition - player_center);

            if (delta.x < 1.0f && delta.y < 1.3f && delta.z < 1.0f)
            {
                pickup.mActive = false;
                Park(pickup.mEntity);
                m_PowerUpTimers[index] = cPowerUpDuration;
                AddPopup(cPowerUps[index].mName, Vec4(cPowerUps[index].mColor, 1.0f));
            }
            else if (pickup.mPosition.z < m_PlayerZ - cRecycleDistance)
            {
                pickup.mActive = false;
                Park(pickup.mEntity);
            }
        }
    }

    Obstacle* AcquireObstacle(int inType)
    {
        for (Obstacle& obstacle : m_Obstacles)
        {
            if (obstacle.mType == inType && !obstacle.mActive)
                return &obstacle;
        }

        return nullptr;
    }

    bool PlaceTrain(int inLane, float inZ, bool inWithRamp, bool inMoving)
    {
        Obstacle* train = AcquireObstacle(OBSTACLE_TRAIN);
        Obstacle* ramp = inWithRamp ? AcquireObstacle(OBSTACLE_RAMP) : nullptr;

        if (train == nullptr || ( inWithRamp && ramp == nullptr ))
            return false;

        const float length = float(RandomInt(2, 4)) * 6.0f;
        const float front = inWithRamp ? inZ + cRampLength : inZ;

        train->mActive = true;
        train->mMoving = inMoving;
        train->mLane = inLane;
        train->mZ = front;
        train->mLength = length;

        SetMaterial(train->mParts[0], m_TrainMaterials[RandomInt(0, cTrainColorCount - 1)]);
        SetTransform(train->mParts[0], Vec3(0.0f, cTrainHeight * 0.5f + 0.1f, length * 0.5f), Vec3(cTrainWidth, cTrainHeight - 0.2f, length - 0.1f));
        SetTransform(train->mParts[1], Vec3(0.0f, cTrainHeight * 0.62f, length * 0.5f), Vec3(cTrainWidth + 0.04f, 0.7f, length - 1.2f));
        SetTransform(train->mParts[2], Vec3(0.0f, cTrainHeight + 0.02f, length * 0.5f), Vec3(cTrainWidth - 0.3f, 0.08f, length - 0.6f));
        SetMaterial(train->mParts[3], inMoving ? m_HeadlightMaterial : m_DarkLightMaterial);
        SetMaterial(train->mParts[4], inMoving ? m_HeadlightMaterial : m_DarkLightMaterial);

        if (ramp != nullptr)
        {
            ramp->mActive = true;
            ramp->mMoving = false;
            ramp->mLane = inLane;
            ramp->mZ = inZ;
        }

        const float meet_offset = inMoving ? cActivationDistance * cOncomingSpeed / ( m_Speed + cOncomingSpeed ) : 0.0f;
        m_LaneBlockedUntil[inLane] = glm::max(m_LaneBlockedUntil[inLane], front + length + meet_offset + 4.0f);

        if (inMoving)
            train->mZ += meet_offset;

        if (inWithRamp)
        {
            for (int coin = 0; coin < 5; coin++)
            {
                const float z = inZ + 1.0f + coin * 2.0f;
                SpawnCoin(Vec3(GetLaneX(inLane), cTrainHeight * glm::min(( z - inZ ) / cRampLength, 1.0f) + 1.0f, z));
            }

            for (float z = front + 2.0f; z < front + length - 1.0f; z += 2.5f)
                SpawnCoin(Vec3(GetLaneX(inLane), cTrainHeight + 1.0f, z));
        }

        return true;
    }

    bool PlaceBarrier(int inType, int inLane, float inZ)
    {
        Obstacle* barrier = AcquireObstacle(inType);

        if (barrier == nullptr)
            return false;

        barrier->mActive = true;
        barrier->mMoving = false;
        barrier->mLane = inLane;
        barrier->mZ = inZ;

        m_LaneBlockedUntil[inLane] = glm::max(m_LaneBlockedUntil[inLane], inZ + 3.0f);
        return true;
    }

    void SpawnCoin(const Vec3& inPosition)
    {
        for (Coin& coin : m_Coins)
        {
            if (coin.mActive)
                continue;

            coin.mActive = true;
            coin.mAttracted = false;
            coin.mPosition = inPosition;
            return;
        }
    }

    bool IsLaneFree(int inLane, float inZ) const
    {
        return m_LaneBlockedUntil[inLane] < inZ;
    }

    void GenerateRow(float inZ)
    {
        Array<int> candidates;

        for (int offset : { -1, 0, 1 })
        {
            const int lane = m_SafeLane + offset;

            if (lane >= 0 && lane < cLaneCount && IsLaneFree(lane, inZ - 6.0f))
                candidates.push_back(lane);
        }

        if (!candidates.empty())
            m_SafeLane = candidates[RandomInt(0, int(candidates.size()) - 1)];

        const float difficulty = glm::clamp(m_RunTime / 90.0f, 0.0f, 1.0f);

        for (int lane = 0; lane < cLaneCount; lane++)
        {
            if (!IsLaneFree(lane, inZ))
                continue;

            const float roll = RandomFloat(0.0f, 1.0f);

            if (lane == m_SafeLane)
            {
                if (roll < 0.25f + 0.2f * difficulty && PlaceBarrier(OBSTACLE_LOW_BARRIER, lane, inZ))
                {
                    for (int coin = 0; coin < 5; coin++)
                    {
                        const float t = float(coin - 2) / 2.0f;
                        SpawnCoin(Vec3(GetLaneX(lane), 1.0f + 1.8f * ( 1.0f - t * t ), inZ + t * 3.5f));
                    }
                }
                else if (roll < 0.45f + 0.25f * difficulty && PlaceBarrier(OBSTACLE_HIGH_BARRIER, lane, inZ))
                {
                    for (int coin = 0; coin < 4; coin++)
                        SpawnCoin(Vec3(GetLaneX(lane), 0.6f, inZ - 3.0f + coin * 2.0f));
                }
                else if (m_DistanceSincePowerUp > 300.0f && RandomFloat(0.0f, 1.0f) < 0.4f)
                {
                    SpawnPowerUp(lane, inZ);
                }
                else
                {
                    for (int coin = 0; coin < 6; coin++)
                        SpawnCoin(Vec3(GetLaneX(lane), 1.0f, inZ + coin * 2.0f));
                }

                continue;
            }

            if (roll < 0.38f)
                PlaceTrain(lane, inZ, RandomFloat(0.0f, 1.0f) < 0.45f, false);
            else if (roll < 0.38f + 0.2f * difficulty)
                PlaceTrain(lane, inZ, false, true);
            else if (roll < 0.68f)
                PlaceBarrier(OBSTACLE_LOW_BARRIER, lane, inZ);
            else if (roll < 0.85f)
                PlaceBarrier(OBSTACLE_HIGH_BARRIER, lane, inZ);
        }
    }

    void SpawnPowerUp(int inLane, float inZ)
    {
        const int type = RandomInt(0, POWER_UP_COUNT - 1);
        PowerUpPickup& pickup = m_PowerUpPickups[type];

        if (pickup.mActive)
            return;

        pickup.mActive = true;
        pickup.mPosition = Vec3(GetLaneX(inLane), 1.1f, inZ);
        m_DistanceSincePowerUp = 0.0f;
    }

    void UpdateGeneration()
    {
        while (m_NextRowZ < m_PlayerZ + cGenerateDistance)
        {
            GenerateRow(m_NextRowZ);
            m_NextRowZ += glm::clamp(m_Speed * 1.15f, 15.0f, 30.0f) + RandomFloat(0.0f, 6.0f);
        }
    }

    void UpdateSegments()
    {
        for (Segment& segment : m_Segments)
        {
            if (segment.mZ + cSegmentLength < m_PlayerZ - 20.0f)
            {
                segment.mZ += cSegmentCount * cSegmentLength;
                RandomizeSegment(segment);
            }
        }
    }

    void UpdateVisuals(float inDeltaTime)
    {
        const bool running = m_State == STATE_RUNNING;
        const bool sliding = running && m_SlideTimer > 0.0f;

        if (running && m_Grounded)
            m_RunPhase += inDeltaTime * ( 6.0f + m_Speed * 0.35f );

        const float swing = glm::sin(m_RunPhase);
        const float bounce = m_BounceTimer > 0.0f ? glm::sin(m_BounceTimer / 0.25f * glm::pi<float>()) * 0.6f * m_BounceDirection : 0.0f;
        const float bob = running && m_Grounded && !sliding ? glm::abs(swing) * 0.08f : 0.0f;

        float pitch = 0.0f;

        if (m_State == STATE_CRASHED || m_State == STATE_GAME_OVER)
            pitch = -glm::min(( m_Time - m_CrashTime ) * 6.0f, 1.45f);

        SetTransform(m_PlayerEntity, Vec3(m_PlayerX - bounce, m_PlayerY + bob, m_PlayerZ), Vec3(1.0f), Quat(Vec3(pitch, 0.0f, 0.0f)));
        SetTransform(m_BodyPivot, Vec3(0.0f, sliding ? 0.25f : 0.0f, 0.0f), Vec3(1.0f), Quat(Vec3(sliding ? -1.2f : 0.0f, 0.0f, 0.0f)));

        for (int side = 0; side < 2; side++)
        {
            const float direction = side == 0 ? 1.0f : -1.0f;
            float leg = 0.0f;
            float arm = 0.0f;

            if (sliding)
            {
                leg = 1.1f;
                arm = -0.4f;
            }
            else if (running && !m_Grounded)
            {
                leg = side == 0 ? 0.9f : -0.3f;
                arm = -2.4f;
            }
            else if (running)
            {
                leg = swing * 0.9f * direction;
                arm = -swing * 0.8f * direction;
            }

            SetTransform(m_LegPivots[side], Vec3(side == 0 ? 0.15f : -0.15f, 0.8f, 0.0f), Vec3(1.0f), Quat(Vec3(leg, 0.0f, 0.0f)));
            SetTransform(m_ArmPivots[side], Vec3(side == 0 ? 0.36f : -0.36f, 1.38f, 0.0f), Vec3(1.0f), Quat(Vec3(arm, 0.0f, side == 0 ? 0.12f : -0.12f)));
            SetMaterial(m_Sneakers[side], m_PowerUpTimers[POWER_UP_SNEAKERS] > 0.0f ? m_GlowingSneakerMaterial : m_SneakerMaterial);
        }

        for (const Obstacle& obstacle : m_Obstacles)
        {
            if (obstacle.mActive)
                SetTransform(obstacle.mEntity, Vec3(GetLaneX(obstacle.mLane), 0.0f, obstacle.mZ));
        }

        for (const Coin& coin : m_Coins)
        {
            if (coin.mActive)
                SetTransform(coin.mEntity, coin.mPosition, Vec3(0.6f, 0.6f, 0.14f), Quat(Vec3(0.0f, m_Time * 4.0f + coin.mPosition.z * 0.3f, 0.0f)));
        }

        for (const PowerUpPickup& pickup : m_PowerUpPickups)
        {
            if (pickup.mActive)
                SetTransform(pickup.mEntity, pickup.mPosition + Vec3(0.0f, 0.15f * glm::sin(m_Time * 3.0f), 0.0f), Vec3(0.8f), Quat(Vec3(0.0f, m_Time * 2.0f, 0.0f)));
        }
    }

    void UpdateCamera(float inDeltaTime)
    {
        Transform* transform = FindComponent<Transform>(m_CameraEntity);

        if (transform == nullptr)
            return;

        const float smoothing = 1.0f - glm::exp(-inDeltaTime * 8.0f);

        Vec3 target_position = Vec3(m_PlayerX * 0.75f, m_PlayerY * 0.5f + 5.6f, m_PlayerZ - 6.2f);

        if (m_State == STATE_READY)
            target_position = Vec3(m_PlayerX + 2.5f * glm::sin(m_Time * 0.5f), 2.4f, m_PlayerZ - 5.5f);

        m_CameraPosition.x = glm::mix(m_CameraPosition.x, target_position.x, smoothing);
        m_CameraPosition.y = glm::mix(m_CameraPosition.y, target_position.y, 1.0f - glm::exp(-inDeltaTime * 5.0f));
        m_CameraPosition.z = m_State == STATE_RUNNING ? target_position.z : glm::mix(m_CameraPosition.z, target_position.z, smoothing);
        m_CameraLookY = glm::mix(m_CameraLookY, m_State == STATE_READY ? 1.2f : m_PlayerY * 0.5f + 0.4f, 1.0f - glm::exp(-inDeltaTime * 5.0f));

        m_Shake = glm::max(0.0f, m_Shake - inDeltaTime * 1.5f);
        const Vec3 shake = Vec3(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f), 0.0f) * m_Shake * m_Shake * 0.5f;

        const Vec3 position = m_CameraPosition + shake;
        const Vec3 look_at = Vec3(m_CameraPosition.x * 0.8f, m_CameraLookY, m_PlayerZ + ( m_State == STATE_READY ? 6.0f : 4.5f ));

        transform->position = position;
        transform->rotation = glm::quatLookAtRH(glm::normalize(look_at - position), Camera::cUp);
        transform->Compose();

        if (Camera* camera = FindComponent<Camera>(m_CameraEntity))
            camera->SetFov(56.0f + 8.0f * ( m_Speed - cStartSpeed ) / ( cMaxSpeed - cStartSpeed ));
    }

    void AddPopup(const String& inText, const Vec4& inColor)
    {
        m_Popups.push_back(Popup { .mText = inText, .mTime = 0.0f, .mColor = inColor });
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

    void DrawPanel(Vec2 inPos, Vec2 inSize, float inScale, Vec4 inBorder = Vec4(1.0f, 0.85f, 0.3f, 0.45f))
    {
        g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 6.0f) * inScale, inSize, 14.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.3f), 14.0f * inScale);
        g_UIRenderer.AddRectFilled(inPos, inSize, 14.0f * inScale, Vec4(0.05f, 0.07f, 0.12f, 0.78f));
        g_UIRenderer.AddRect(inPos, inSize, 14.0f * inScale, 1.5f * inScale, inBorder);
    }

    void DrawOutlinedText(Vec2 inPos, StringView inText, float inSize, Vec4 inColor, EUITextAlign inAlign, float inScale)
    {
        g_UIRenderer.AddText(inPos + Vec2(2.0f, 3.0f) * inScale, inText, inSize, Vec4(0.0f, 0.0f, 0.0f, 0.55f * inColor.a), inAlign);
        g_UIRenderer.AddText(inPos, inText, inSize, inColor, inAlign);
    }

    void DrawHUD(float inDeltaTime)
    {
        const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
        const float scale = glm::max(display.y / 1080.0f, 0.5f);
        const float margin = 28.0f * scale;

        for (const PowerUpPickup& pickup : m_PowerUpPickups)
        {
            if (!pickup.mActive)
                continue;

            Vec2 screen;
            if (ProjectToScreen(pickup.mPosition + Vec3(0.0f, 0.9f, 0.0f), screen))
            {
                const int index = int(&pickup - m_PowerUpPickups.data());
                DrawOutlinedText(screen, cPowerUps[index].mLabel, 26.0f * scale, Vec4(1.0f), UI_TEXT_ALIGN_CENTER, scale);
            }
        }

        DrawCheats(display, scale, margin);

        if (m_State == STATE_READY)
        {
            DrawReady(display, scale);
            return;
        }

        const Vec2 score_pos = Vec2(display.x - margin - 260.0f * scale, margin);
        DrawPanel(score_pos, Vec2(260.0f, 120.0f) * scale, scale);
        g_UIRenderer.AddText(score_pos + Vec2(240.0f, 10.0f) * scale, "SCORE", 16.0f * scale, Vec4(1.0f, 0.85f, 0.4f, 1.0f), UI_TEXT_ALIGN_RIGHT);
        g_UIRenderer.AddText(score_pos + Vec2(240.0f, 30.0f) * scale, std::format("{}", int(m_Score)), 44.0f * scale, Vec4(1.0f), UI_TEXT_ALIGN_RIGHT);

        g_UIRenderer.AddCircleFilled(score_pos + Vec2(150.0f, 96.0f) * scale, 9.0f * scale, Vec4(1.0f, 0.78f, 0.2f, 1.0f));
        g_UIRenderer.AddText(score_pos + Vec2(240.0f, 84.0f) * scale, std::format("{}", m_CoinCount), 22.0f * scale, Vec4(1.0f, 0.85f, 0.4f, 1.0f), UI_TEXT_ALIGN_RIGHT);

        g_UIRenderer.AddText(Vec2(margin, margin), std::format("{} m", int(m_PlayerZ)), 30.0f * scale, Vec4(1.0f, 1.0f, 1.0f, 0.9f));

        if (m_PowerUpTimers[POWER_UP_MULTIPLIER] > 0.0f)
            DrawOutlinedText(Vec2(margin, margin + 40.0f * scale), "2X SCORE", 22.0f * scale, Vec4(1.0f, 0.8f, 0.2f, 1.0f), UI_TEXT_ALIGN_LEFT, scale);

        float bar_y = display.y - margin - 30.0f * scale;

        for (int index = 0; index < POWER_UP_COUNT; index++)
        {
            const float timer = m_PowerUpTimers[index];

            if (timer <= 0.0f)
                continue;

            const Vec2 pos = Vec2(margin, bar_y);
            const Vec2 size = Vec2(240.0f, 22.0f) * scale;
            const Vec4 color = Vec4(cPowerUps[index].mColor, 1.0f);

            g_UIRenderer.AddRectFilled(pos, size, size.y * 0.5f, Vec4(0.0f, 0.0f, 0.0f, 0.55f));
            g_UIRenderer.AddRectFilled(pos, Vec2(glm::max(size.x * timer / cPowerUpDuration, size.y), size.y), size.y * 0.5f, color);
            g_UIRenderer.AddText(pos + Vec2(10.0f, 2.0f) * scale, cPowerUps[index].mName, 15.0f * scale, Vec4(0.05f, 0.05f, 0.08f, 1.0f));

            bar_y -= 32.0f * scale;
        }

        if (m_GuardTimer > 0.0f && m_State == STATE_RUNNING)
        {
            const float pulse = 0.5f + 0.5f * glm::sin(m_Time * 10.0f);
            g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 50.0f * scale, Vec4(1.0f, 0.2f, 0.1f, 0.15f + 0.15f * pulse));
            DrawOutlinedText(Vec2(display.x * 0.5f, display.y - margin - 50.0f * scale), "ONE MORE STUMBLE AND YOU'RE CAUGHT", 22.0f * scale, Vec4(1.0f, 0.45f, 0.35f, 0.8f + 0.2f * pulse), UI_TEXT_ALIGN_CENTER, scale);
        }

        float popup_y = display.y * 0.22f;

        for (Popup& popup : m_Popups)
        {
            popup.mTime += inDeltaTime;

            const float fade_in = glm::clamp(popup.mTime / 0.12f, 0.0f, 1.0f);
            const float fade_out = glm::clamp(( 1.6f - popup.mTime ) / 0.4f, 0.0f, 1.0f);
            const float size = glm::mix(30.0f, 44.0f, fade_in) * scale;

            DrawOutlinedText(Vec2(display.x * 0.5f, popup_y), popup.mText, size, Vec4(Vec3(popup.mColor), fade_in * fade_out), UI_TEXT_ALIGN_CENTER, scale);
            popup_y += 52.0f * scale;
        }

        std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= 1.6f; });

        if (m_State == STATE_GAME_OVER)
            DrawGameOver(display, scale);
    }

    void DrawCheats(const Vec2& inDisplay, float inScale, float inMargin)
    {
        String text;

        if (*m_GodMode)
            text += "GOD MODE";

        if (*m_SpeedCheat > 0.0f)
            text += std::format("{}SPEED {} M/S", text.empty() ? "" : "   ", int(*m_SpeedCheat));

        if (text.empty())
            return;

        DrawOutlinedText(Vec2(inDisplay.x - inMargin, inDisplay.y - inMargin - 26.0f * inScale), "CHEATS   " + text, 18.0f * inScale, Vec4(0.6f, 0.9f, 1.0f, 0.9f), UI_TEXT_ALIGN_RIGHT, inScale);
    }

    void DrawReady(const Vec2& inDisplay, float inScale)
    {
        const float center = inDisplay.x * 0.5f;

        DrawOutlinedText(Vec2(center, inDisplay.y * 0.16f), "RAIL RUSH", 96.0f * inScale, Vec4(1.0f, 0.85f, 0.25f, 1.0f), UI_TEXT_ALIGN_CENTER, inScale);
        DrawOutlinedText(Vec2(center, inDisplay.y * 0.16f + 110.0f * inScale), "Dodge the trains. Grab the coins. Don't get caught.", 24.0f * inScale, Vec4(1.0f), UI_TEXT_ALIGN_CENTER, inScale);

        static constexpr std::array<std::pair<const char*, const char*>, 5> cControls =
        {
            std::pair { "A / D",           "Switch lanes" },
            std::pair { "W / SPACE",       "Jump" },
            std::pair { "S",               "Slide, or slam down mid-air" },
            std::pair { "F1 / F2",         "Cheats: god mode / speed" },
            std::pair { "ESC",             "Pause" },
        };

        const float line_height = 30.0f * inScale;
        const Vec2 panel_size = Vec2(460.0f * inScale, cControls.size() * line_height + 30.0f * inScale);
        const Vec2 panel_pos = Vec2(center - panel_size.x * 0.5f, inDisplay.y * 0.62f);

        DrawPanel(panel_pos, panel_size, inScale);

        for (size_t index = 0; index < cControls.size(); index++)
        {
            const Vec2 pos = panel_pos + Vec2(30.0f * inScale, 15.0f * inScale + index * line_height);
            g_UIRenderer.AddText(pos, cControls[index].first, 20.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, 1.0f));
            g_UIRenderer.AddText(pos + Vec2(150.0f * inScale, 0.0f), cControls[index].second, 20.0f * inScale, Vec4(0.9f, 0.92f, 1.0f, 0.95f));
        }

        const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
        DrawOutlinedText(Vec2(center, panel_pos.y + panel_size.y + 30.0f * inScale), "Press SPACE to run", 30.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, blink), UI_TEXT_ALIGN_CENTER, inScale);

        if (m_BestScore > 0)
            g_UIRenderer.AddText(Vec2(center, panel_pos.y + panel_size.y + 80.0f * inScale), std::format("Best {}", m_BestScore), 20.0f * inScale, Vec4(1.0f, 1.0f, 1.0f, 0.8f), UI_TEXT_ALIGN_CENTER);
    }

    void DrawGameOver(const Vec2& inDisplay, float inScale)
    {
        const float alpha = glm::clamp(( m_Time - m_CrashTime - cCrashDuration ) / 0.4f, 0.0f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.02f, 0.5f * alpha));

        const Vec2 size = Vec2(560.0f, 300.0f) * inScale;
        const Vec2 pos = inDisplay * 0.5f - size * 0.5f;

        g_UIRenderer.AddRectFilled(pos, size, 20.0f * inScale, Vec4(0.05f, 0.07f, 0.12f, 0.92f * alpha));
        g_UIRenderer.AddRect(pos, size, 20.0f * inScale, 2.0f * inScale, Vec4(1.0f, 0.85f, 0.3f, 0.8f * alpha));

        const float center = inDisplay.x * 0.5f;
        g_UIRenderer.AddText(Vec2(center, pos.y + 24.0f * inScale), m_CrashReason, 40.0f * inScale, Vec4(1.0f, 0.45f, 0.35f, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(center, pos.y + 86.0f * inScale), std::format("{}", int(m_Score)), 64.0f * inScale, Vec4(1.0f, 1.0f, 1.0f, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(center, pos.y + 170.0f * inScale), std::format("{} m   {} coins   best {}", int(m_PlayerZ), m_CoinCount, m_BestScore), 22.0f * inScale, Vec4(1.0f, 0.85f, 0.5f, alpha), UI_TEXT_ALIGN_CENTER);

        const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
        g_UIRenderer.AddText(Vec2(center, pos.y + 232.0f * inScale), "Press SPACE to run again", 22.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, alpha * blink), UI_TEXT_ALIGN_CENTER);
    }

private:
    bool m_Started = false;
    EState m_State = STATE_READY;
    float m_Time = 0.0f;
    float m_RunTime = 0.0f;
    std::mt19937 m_Random;

    Entity m_RootEntity = Entity::Null;
    Entity m_CameraEntity = Entity::Null;
    Entity m_PlayerEntity = Entity::Null;
    Entity m_BodyPivot = Entity::Null;
    StaticArray<Entity, 2> m_LegPivots;
    StaticArray<Entity, 2> m_ArmPivots;
    StaticArray<Entity, 2> m_Sneakers;

    StaticArray<Entity, cTrainColorCount> m_TrainMaterials;
    StaticArray<Entity, cBuildingColorCount> m_BuildingMaterials;
    StaticArray<Entity, POWER_UP_COUNT> m_PowerUpMaterials;
    Entity m_GravelMaterial = Entity::Null;
    Entity m_ConcreteMaterial = Entity::Null;
    Entity m_TieMaterial = Entity::Null;
    Entity m_RailMaterial = Entity::Null;
    Entity m_WindowMaterial = Entity::Null;
    Entity m_GlassMaterial = Entity::Null;
    Entity m_LampMaterial = Entity::Null;
    Entity m_PostMaterial = Entity::Null;
    Entity m_TrainWindowMaterial = Entity::Null;
    Entity m_HeadlightMaterial = Entity::Null;
    Entity m_DarkLightMaterial = Entity::Null;
    Entity m_RampMaterial = Entity::Null;
    Entity m_BarrierMaterial = Entity::Null;
    Entity m_StripeMaterial = Entity::Null;
    Entity m_HighBarrierMaterial = Entity::Null;
    Entity m_CoinMaterial = Entity::Null;
    Entity m_HoodieMaterial = Entity::Null;
    Entity m_PantsMaterial = Entity::Null;
    Entity m_SkinMaterial = Entity::Null;
    Entity m_CapMaterial = Entity::Null;
    Entity m_BackpackMaterial = Entity::Null;
    Entity m_SneakerMaterial = Entity::Null;
    Entity m_GlowingSneakerMaterial = Entity::Null;

    StaticArray<Segment, cSegmentCount> m_Segments;
    Array<Obstacle> m_Obstacles;
    Array<Coin> m_Coins;
    StaticArray<PowerUpPickup, POWER_UP_COUNT> m_PowerUpPickups;

    float m_Speed = cStartSpeed;
    float m_PlayerZ = 0.0f;
    float m_PlayerX = 0.0f;
    float m_PlayerY = 0.0f;
    float m_VelocityY = 0.0f;
    int m_Lane = 1;
    int m_PreviousLane = 1;
    bool m_Grounded = true;
    float m_SlideTimer = 0.0f;
    float m_BounceTimer = 0.0f;
    float m_BounceDirection = 0.0f;
    float m_GuardTimer = 0.0f;
    float m_CrashTime = 0.0f;
    const char* m_CrashReason = "";
    float m_RunPhase = 0.0f;

    float m_Score = 0.0f;
    int m_BestScore = 0;
    int m_CoinCount = 0;
    StaticArray<float, POWER_UP_COUNT> m_PowerUpTimers = {};

    int m_SafeLane = 1;
    float m_NextRowZ = 0.0f;
    StaticArray<float, cLaneCount> m_LaneBlockedUntil = {};
    float m_DistanceSincePowerUp = 0.0f;

    Array<Popup> m_Popups;

    int* m_GodMode = nullptr;
    int* m_AutoStart = nullptr;
    float* m_SpeedCheat = nullptr;

    Vec3 m_CameraPosition = Vec3(0.0f);
    float m_CameraLookY = 1.2f;
    float m_Shake = 0.0f;

public:
    RailRushScript()
    {
        m_LegPivots.fill(Entity::Null);
        m_ArmPivots.fill(Entity::Null);
        m_Sneakers.fill(Entity::Null);
        m_TrainMaterials.fill(Entity::Null);
        m_BuildingMaterials.fill(Entity::Null);
        m_PowerUpMaterials.fill(Entity::Null);
    }
};


RTTI_DEFINE_TYPE(RailRushScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(RailRushScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(RailRushScript, SERIALIZE_ALL, "Best Score", m_BestScore);
}

RK_REGISTER_SCRIPT(RailRushScript)

} // RK
