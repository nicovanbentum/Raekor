#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"
#include "../Engine/GLTF.h"

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
static constexpr int   cBuildingsPerSide = 2;
static constexpr int   cPropsPerSide = 2;
static constexpr float cTrackHalfWidth = ( cLaneCount * cLaneWidth + 1.0f ) * 0.5f;
static constexpr float cPlatformTop = 0.85f;
static constexpr float cGenerateDistance = 220.0f;
static constexpr float cRecycleDistance = 15.0f;

static constexpr int   cTrainPoolSize = 18;
static constexpr int   cRampPoolSize = 8;
static constexpr int   cBarrierPoolSize = 14;
static constexpr int   cCoinPoolSize = 180;
static constexpr int   cMaxTrainUnits = 4;
static constexpr float cTrainUnitLength = 6.0f;

static constexpr float cGuardHiddenZ = -16.0f;
static constexpr float cGuardChaseZ = -2.1f;
static constexpr float cGuardSideOffset = 0.5f;

static constexpr float cSkateboardDuration = 30.0f;
static constexpr int   cSkateboardsPerRun = 3;
static constexpr float cSkateboardScale = 1.5f;
static constexpr float cSkateboardRideHeight = 0.22f;
static constexpr float cSmashInvulnerability = 1.0f;
static constexpr Vec3  cSkateboardColor = Vec3(0.35f, 0.85f, 1.0f);

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -60.0f, 0.0f);

static constexpr const char* cRuntimeRootName = "RailRush Runtime";
static constexpr const char* cModelFile = "Assets/RailRush/RailRush.glb";
static constexpr const char* cPaletteFile = "Assets/RailRush/Palette.png";
static constexpr const char* cPaletteEmissiveFile = "Assets/RailRush/PaletteEmissive.png";

static constexpr StaticArray<const char*, 4> cTrainColors = { "Blue", "Red", "Green", "Silver" };
static constexpr StaticArray<const char*, 7> cBuildingNames = { "Apartment", "Tower", "ShopRed", "ShopGreen", "Warehouse", "Office", "Brownstone" };
static constexpr StaticArray<const char*, 5> cPropNames = { "Bench", "TrashCan", "Vending", "Sign", "Planter" };


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
    const char* mModel;
    Vec3 mColor;
};


static const StaticArray<PowerUpInfo, POWER_UP_COUNT> cPowerUps =
{
    PowerUpInfo { "MAGNET",         "M",  "Magnet",     Vec3(1.0f, 0.25f, 0.25f) },
    PowerUpInfo { "SUPER SNEAKERS", "S",  "Sneaker",    Vec3(0.3f, 1.0f, 0.45f) },
    PowerUpInfo { "2X SCORE",       "2X", "Multiplier", Vec3(1.0f, 0.8f, 0.2f) },
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

    struct ModelPart
    {
        String mName;
        Entity mEntity = Entity::Null;
        Vec3 mRest = Vec3(0.0f);
    };

    struct Model
    {
        Entity mRoot = Entity::Null;
        Array<ModelPart> mParts;
    };

    struct PartTemplate
    {
        String mName;
        Vec3 mJoint = Vec3(0.0f);
        Mesh mMesh;
    };

    struct Obstacle
    {
        Entity mEntity = Entity::Null;
        StaticArray<Model, cMaxTrainUnits> mUnits;
        Entity mLights = Entity::Null;
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
        float mSpinPhase = 0.0f;
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
        StaticArray<Entity, cPropsPerSide * 2> mProps;
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

        LoadTextures();
        LoadModels();
        CreateMaterials();
        CreateSegments();
        CreatePlayer();
        CreateGuard();
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

        if (key == SDLK_G)
        {
            *m_GodMode = !*m_GodMode;
            AddPopup(*m_GodMode ? "GOD MODE ON" : "GOD MODE OFF", Vec4(0.6f, 0.9f, 1.0f, 1.0f));
            return;
        }

        if (key == SDLK_F)
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
            case SDLK_E:     UseSkateboard(); break;
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

    Entity CreateShape(StringView inName, Entity inMaterial, Vec3 inPosition, Vec3 inScale, Entity inParent = Entity::Null)
    {
        const Entity entity = CreateEntity(inName, inParent);
        SetTransform(entity, inPosition, inScale);

        Mesh& mesh = m_Scene->Add<Mesh>(entity);
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

    uint32_t LoadTexture(const char* inFile, bool inSRGB)
    {
        if (!fs::exists(inFile))
            return 0;

        const String cached = TextureAsset::GetCachedPath(inFile);

        if (!fs::exists(cached) || fs::last_write_time(cached) < fs::last_write_time(inFile))
            TextureAsset::Convert(inFile);

        Assets* assets = m_App->GetAssets();
        TextureAsset::Ptr asset = assets ? assets->GetAsset<TextureAsset>(cached) : nullptr;

        return asset ? m_App->GetRenderInterface()->UploadTextureFromAsset(asset, inSRGB) : 0;
    }

    void LoadTextures()
    {
        if (m_PaletteTexture == 0)
            m_PaletteTexture = LoadTexture(cPaletteFile, true);

        if (m_PaletteEmissiveTexture == 0)
            m_PaletteEmissiveTexture = LoadTexture(cPaletteEmissiveFile, false);
    }

    void ReleaseModels()
    {
        for (auto& [name, templates] : m_MeshTemplates)
        {
            for (PartTemplate& part : templates)
                m_App->GetRenderInterface()->DestroyMeshBuffers(Entity::Null, part.mMesh);
        }

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

            PartTemplate& part = m_MeshTemplates[name.substr(0, split)].emplace_back();
            part.mName = split != String::npos ? name.substr(split + 1) : String("Main");
            part.mJoint = scene.Get<Transform>(entity).position;
            part.mMesh = mesh;

            m_App->GetRenderInterface()->UploadMeshBuffers(Entity::Null, part.mMesh);
        }
    }

    const PartTemplate* FindTemplate(const String& inName) const
    {
        const auto templates = m_MeshTemplates.find(inName);
        return templates != m_MeshTemplates.end() && !templates->second.empty() ? &templates->second.front() : nullptr;
    }

    void SetMesh(Entity inEntity, const PartTemplate* inTemplate)
    {
        Mesh* mesh = FindComponent<Mesh>(inEntity);

        if (mesh == nullptr || inTemplate == nullptr)
            return;

        const Entity material = mesh->material;

        m_App->GetRenderInterface()->DestroyMeshBuffers(inEntity, *mesh);

        *mesh = inTemplate->mMesh;
        mesh->material = material;
        mesh->indexBuffer = 0;
        mesh->vertexBuffer = 0;
        mesh->BottomLevelAS = 0;

        m_App->GetRenderInterface()->ShareMeshBuffers(inEntity, inTemplate->mMesh, *mesh);
    }

    Entity CreateMeshEntity(StringView inName, const PartTemplate* inTemplate, Entity inParent)
    {
        const Entity entity = CreateEntity(inName, inParent);
        m_Scene->Add<Mesh>(entity).material = m_PaletteMaterial;
        SetMesh(entity, inTemplate);
        return entity;
    }

    Model CreateModel(const String& inName, Entity inParent = Entity::Null)
    {
        Model model;
        model.mRoot = CreateEntity(inName, inParent);

        const auto templates = m_MeshTemplates.find(inName);

        if (templates == m_MeshTemplates.end())
        {
            const Entity fallback = CreateShape(inName, m_PaletteMaterial, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f), model.mRoot);
            model.mParts.push_back(ModelPart { .mName = "Main", .mEntity = fallback, .mRest = Vec3(0.0f, 0.5f, 0.0f) });
            return model;
        }

        for (const PartTemplate& part : templates->second)
        {
            const Entity entity = CreateMeshEntity(part.mName, &part, model.mRoot);
            SetTransform(entity, part.mJoint);
            model.mParts.push_back(ModelPart { .mName = part.mName, .mEntity = entity, .mRest = part.mJoint });
        }

        return model;
    }

    static ModelPart FindPart(const Model& inModel, StringView inName)
    {
        for (const ModelPart& part : inModel.mParts)
        {
            if (part.mName == inName)
                return part;
        }

        return ModelPart();
    }

    void CreateMaterials()
    {
        m_PaletteMaterial = CreateMaterial(Vec3(1.0f), Vec3(0.0f), 0.85f);

        if (Material* palette = FindComponent<Material>(m_PaletteMaterial))
        {
            palette->gpuAlbedoMap = m_PaletteTexture;
            palette->gpuEmissiveMap = m_PaletteEmissiveTexture;
            palette->emissive = m_PaletteEmissiveTexture != 0 ? Vec3(20000.0f) : Vec3(0.0f);
        }

        m_HeadlightMaterial = CreateMaterial(Vec3(1.0f, 0.95f, 0.8f), Vec3(1.0f, 0.95f, 0.8f) * 60000.0f, 0.2f);
        m_DarkLightMaterial = CreateMaterial(Vec3(0.2f, 0.2f, 0.2f), Vec3(0.0f), 0.3f);
        m_GlowingSneakerMaterial = CreateMaterial(Vec3(0.3f, 1.0f, 0.45f), Vec3(0.3f, 1.0f, 0.45f) * 8000.0f, 0.3f);
    }

    void CreateSegments()
    {
        for (Segment& segment : m_Segments)
        {
            segment.mRoot = CreateEntity("Track Segment");

            CreateModel("Track", segment.mRoot);
            CreateModel("PlatformL", segment.mRoot);
            CreateModel("PlatformR", segment.mRoot);

            SetTransform(CreateModel("LampPostL", segment.mRoot).mRoot, Vec3(cTrackHalfWidth + 1.2f, cPlatformTop, 4.0f));
            SetTransform(CreateModel("LampPostR", segment.mRoot).mRoot, Vec3(-cTrackHalfWidth - 1.2f, cPlatformTop, 19.0f));

            for (Entity& building : segment.mBuildings)
                building = CreateMeshEntity("Building", nullptr, segment.mRoot);

            for (Entity& prop : segment.mProps)
                prop = CreateMeshEntity("Prop", nullptr, segment.mRoot);
        }
    }

    void RandomizeSegment(Segment& inSegment)
    {
        const float building_length = cSegmentLength / cBuildingsPerSide;

        for (int index = 0; index < cBuildingsPerSide * 2; index++)
        {
            const bool left = index < cBuildingsPerSide;
            const float side = left ? 1.0f : -1.0f;
            const float z = ( float(index % cBuildingsPerSide) + 0.5f ) * building_length;
            const String name = String(cBuildingNames[RandomInt(0, int(cBuildingNames.size()) - 1)]) + ( left ? "L" : "R" );

            SetMesh(inSegment.mBuildings[index], FindTemplate(name));
            SetTransform(inSegment.mBuildings[index], Vec3(side * ( cTrackHalfWidth + 4.2f ), 0.0f, z));
        }

        for (int index = 0; index < cPropsPerSide * 2; index++)
        {
            const Entity prop = inSegment.mProps[index];
            const float side = index < cPropsPerSide ? 1.0f : -1.0f;

            if (RandomFloat(0.0f, 1.0f) < 0.25f)
            {
                Park(prop);
                continue;
            }

            const float slot_length = cSegmentLength / cPropsPerSide;
            const float z = ( float(index % cPropsPerSide) + RandomFloat(0.25f, 0.75f) ) * slot_length;

            SetMesh(prop, FindTemplate(cPropNames[RandomInt(0, int(cPropNames.size()) - 1)]));
            SetTransform(prop, Vec3(side * ( cTrackHalfWidth + RandomFloat(2.6f, 3.2f) ), cPlatformTop, z), Vec3(1.0f), Quat(Vec3(0.0f, -side * glm::half_pi<float>(), 0.0f)));
        }
    }

    void CreatePlayer()
    {
        m_PlayerEntity = CreateEntity("Runner");
        m_BodyPivot = CreateEntity("Body", m_PlayerEntity);
        m_Runner = CreateModel("Runner", m_BodyPivot);

        m_Legs = { FindPart(m_Runner, "LegL"), FindPart(m_Runner, "LegR") };
        m_Arms = { FindPart(m_Runner, "ArmL"), FindPart(m_Runner, "ArmR") };
        m_Shoes = { FindPart(m_Runner, "ShoeL"), FindPart(m_Runner, "ShoeR") };

        m_Skateboard = CreateModel("Skateboard", m_PlayerEntity);
        Park(m_Skateboard.mRoot);
    }

    void CreateGuard()
    {
        m_Guard = CreateModel("Guard");
        Park(m_Guard.mRoot);
    }

    Obstacle CreateObstacle(int inType, int inIndex)
    {
        Obstacle obstacle;
        obstacle.mType = inType;

        switch (inType)
        {
            case OBSTACLE_TRAIN:
            {
                const String color = cTrainColors[inIndex % cTrainColors.size()];

                obstacle.mEntity = CreateEntity("Train");

                for (int unit = 0; unit < cMaxTrainUnits; unit++)
                    obstacle.mUnits[unit] = CreateModel(( unit == 0 ? "TrainFront" : "TrainCar" ) + color, obstacle.mEntity);

                obstacle.mLights = FindPart(obstacle.mUnits[0], "Lights").mEntity;
            } break;

            case OBSTACLE_RAMP:
            {
                obstacle.mLength = cRampLength;
                obstacle.mEntity = CreateModel("Ramp").mRoot;
            } break;

            case OBSTACLE_LOW_BARRIER:
            {
                obstacle.mLength = cBarrierDepth;
                obstacle.mEntity = CreateModel("LowBarrier").mRoot;
            } break;

            case OBSTACLE_HIGH_BARRIER:
            {
                obstacle.mLength = cBarrierDepth;
                obstacle.mEntity = CreateModel("HighBarrier").mRoot;
            } break;
        }

        Park(obstacle.mEntity);
        return obstacle;
    }

    void CreatePools()
    {
        m_Obstacles.clear();

        for (int index = 0; index < cTrainPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_TRAIN, index));

        for (int index = 0; index < cRampPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_RAMP, index));

        for (int index = 0; index < cBarrierPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_LOW_BARRIER, index));

        for (int index = 0; index < cBarrierPoolSize; index++)
            m_Obstacles.push_back(CreateObstacle(OBSTACLE_HIGH_BARRIER, index));

        m_Coins.resize(cCoinPoolSize);
        for (Coin& coin : m_Coins)
        {
            coin.mEntity = CreateModel("Coin").mRoot;
            Park(coin.mEntity);
        }

        for (int index = 0; index < POWER_UP_COUNT; index++)
        {
            m_PowerUpPickups[index].mEntity = CreateModel(cPowerUps[index].mModel).mRoot;
            Park(m_PowerUpPickups[index].mEntity);
        }
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
        m_Runner = Model();
        m_Guard = Model();
        m_Skateboard = Model();
        m_Legs.fill(ModelPart());
        m_Arms.fill(ModelPart());
        m_Shoes.fill(ModelPart());
        m_Obstacles.clear();
        m_Coins.clear();

        for (Segment& segment : m_Segments)
            segment = Segment();

        for (PowerUpPickup& pickup : m_PowerUpPickups)
            pickup = PowerUpPickup();

        ReleaseModels();
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
        m_Distance = 0.0f;
        m_TrackOffset = 0.0f;
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
        m_CaughtByGuard = false;
        m_SkateboardTimer = 0.0f;
        m_InvulnerableTimer = 0.0f;
        m_SkateboardsLeft = cSkateboardsPerRun;
        m_GuardPosition = Vec3(GetLaneX(1), 0.0f, cGuardHiddenZ);
        m_GuardPhase = 0.0f;

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

        const bool blocked = GetSurfaceHeight(target, 0.0f) > m_PlayerY + cStepHeight || GetSurfaceHeight(target, 1.0f) > m_PlayerY + cStepHeight;

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

        if (m_InvulnerableTimer > 0.0f)
            return;

        if (m_SkateboardTimer > 0.0f)
        {
            SmashSkateboard();
            return;
        }

        if (m_GuardTimer > 0.0f)
        {
            m_CaughtByGuard = true;
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

    void UseSkateboard()
    {
        if (m_SkateboardTimer > 0.0f || m_SkateboardsLeft <= 0)
            return;

        m_SkateboardsLeft--;
        m_SkateboardTimer = cSkateboardDuration;
        AddPopup("SKATEBOARD!", Vec4(cSkateboardColor, 1.0f));
    }

    void SmashSkateboard()
    {
        m_SkateboardTimer = 0.0f;
        m_InvulnerableTimer = glm::max(m_InvulnerableTimer, cSmashInvulnerability);
        m_Shake = glm::max(m_Shake, 0.6f);
        AddPopup("BOARD SMASHED!", Vec4(1.0f, 0.55f, 0.3f, 1.0f));
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

    void ScrollWorld(float inDistance)
    {
        m_Distance += inDistance;
        m_TrackOffset -= inDistance;

        while (m_TrackOffset <= -cSegmentLength)
        {
            m_TrackOffset += cSegmentLength;

            for (Segment& segment : m_Segments)
                segment.mZ -= cSegmentLength;
        }

        for (Obstacle& obstacle : m_Obstacles)
            obstacle.mZ -= inDistance;

        for (Coin& coin : m_Coins)
            coin.mPosition.z -= inDistance;

        for (PowerUpPickup& pickup : m_PowerUpPickups)
            pickup.mPosition.z -= inDistance;

        for (float& blocked_until : m_LaneBlockedUntil)
            blocked_until -= inDistance;

        m_NextRowZ -= inDistance;
    }

    void UpdatePlayer(float inDeltaTime)
    {
        m_Speed = *m_SpeedCheat > 0.0f ? *m_SpeedCheat : glm::min(cMaxSpeed, cStartSpeed + m_RunTime * cAcceleration);

        const float step = m_Speed * inDeltaTime;
        const float previous_z = -step;
        ScrollWorld(step);

        const float multiplier = m_PowerUpTimers[POWER_UP_MULTIPLIER] > 0.0f ? 2.0f : 1.0f;
        m_Score += m_Speed * inDeltaTime * multiplier;
        m_DistanceSincePowerUp += m_Speed * inDeltaTime;

        m_PlayerX = glm::mix(m_PlayerX, GetLaneX(m_Lane), 1.0f - glm::exp(-inDeltaTime * 16.0f));

        m_BounceTimer = glm::max(0.0f, m_BounceTimer - inDeltaTime);
        m_GuardTimer = glm::max(0.0f, m_GuardTimer - inDeltaTime);
        m_SlideTimer = glm::max(0.0f, m_SlideTimer - ( m_Grounded ? inDeltaTime : 0.0f ));
        m_SkateboardTimer = glm::max(0.0f, m_SkateboardTimer - inDeltaTime);
        m_InvulnerableTimer = glm::max(0.0f, m_InvulnerableTimer - inDeltaTime);

        for (float& timer : m_PowerUpTimers)
            timer = glm::max(0.0f, timer - inDeltaTime);

        const float previous_y = m_PlayerY;

        m_VelocityY -= cGravity * inDeltaTime;
        m_PlayerY += m_VelocityY * inDeltaTime;

        const int lane = GetNearestLane(m_PlayerX);
        float ground = GetSurfaceHeight(lane, 0.0f);

        const bool blocked = ground > previous_y + cStepHeight;

        if (blocked && m_SkateboardTimer > 0.0f && m_InvulnerableTimer <= 0.0f && !*m_GodMode)
            SmashSkateboard();

        if (blocked && m_InvulnerableTimer > 0.0f)
            m_InvulnerableTimer = glm::max(m_InvulnerableTimer, 0.15f);

        if (blocked && ( *m_GodMode || m_InvulnerableTimer > 0.0f ))
            ground = glm::min(ground, previous_y);

        if (ground > previous_y + cStepHeight)
        {
            Crash("HIT A TRAIN");
            ScrollWorld(-step);
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

            if (*m_GodMode || m_InvulnerableTimer > 0.0f)
                continue;

            if (obstacle.mZ + obstacle.mLength < previous_z - 0.3f || obstacle.mZ > 0.3f)
                continue;

            const float top = m_PlayerY + ( sliding ? cSlideHeight : cPlayerHeight );

            const bool hit_low = obstacle.mType == OBSTACLE_LOW_BARRIER && m_PlayerY < cLowBarrierHeight;
            const bool hit_high = obstacle.mType == OBSTACLE_HIGH_BARRIER && m_PlayerY < cHighBarrierTop && top > cHighBarrierBottom;

            if (!hit_low && !hit_high)
                continue;

            if (m_SkateboardTimer > 0.0f)
            {
                SmashSkateboard();
                continue;
            }

            Crash("HIT A BARRIER");
            return;
        }
    }

    void UpdateObstacles(float inDeltaTime)
    {
        for (Obstacle& obstacle : m_Obstacles)
        {
            if (!obstacle.mActive)
                continue;

            if (obstacle.mMoving && obstacle.mZ < cActivationDistance)
                obstacle.mZ -= cOncomingSpeed * inDeltaTime;

            if (obstacle.mZ + obstacle.mLength < -cRecycleDistance)
            {
                obstacle.mActive = false;
                Park(obstacle.mEntity);
            }
        }
    }

    void UpdatePickups(float inDeltaTime)
    {
        const Vec3 player_center = Vec3(m_PlayerX, m_PlayerY + ( m_SlideTimer > 0.0f ? 0.4f : 0.9f ), 0.0f);
        const bool magnet = m_PowerUpTimers[POWER_UP_MAGNET] > 0.0f;
        const float multiplier = m_PowerUpTimers[POWER_UP_MULTIPLIER] > 0.0f ? 2.0f : 1.0f;

        for (Coin& coin : m_Coins)
        {
            if (!coin.mActive)
                continue;

            if (magnet && !coin.mAttracted && coin.mPosition.z < 14.0f && coin.mPosition.z > -1.0f)
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
            else if (coin.mPosition.z < -cRecycleDistance)
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
            else if (pickup.mPosition.z < -cRecycleDistance)
            {
                pickup.mActive = false;
                Park(pickup.mEntity);
            }
        }
    }

    Obstacle* AcquireObstacle(int inType)
    {
        Array<Obstacle*> candidates;

        for (Obstacle& obstacle : m_Obstacles)
        {
            if (obstacle.mType == inType && !obstacle.mActive)
                candidates.push_back(&obstacle);
        }

        return candidates.empty() ? nullptr : candidates[RandomInt(0, int(candidates.size()) - 1)];
    }

    bool PlaceTrain(int inLane, float inZ, bool inWithRamp, bool inMoving)
    {
        Obstacle* train = AcquireObstacle(OBSTACLE_TRAIN);
        Obstacle* ramp = inWithRamp ? AcquireObstacle(OBSTACLE_RAMP) : nullptr;

        if (train == nullptr || ( inWithRamp && ramp == nullptr ))
            return false;

        const int unit_count = RandomInt(2, cMaxTrainUnits);
        const float length = float(unit_count) * cTrainUnitLength;
        const float front = inWithRamp ? inZ + cRampLength : inZ;

        train->mActive = true;
        train->mMoving = inMoving;
        train->mLane = inLane;
        train->mZ = front;
        train->mLength = length;

        for (int unit = 0; unit < cMaxTrainUnits; unit++)
        {
            if (unit < unit_count)
                SetTransform(train->mUnits[unit].mRoot, Vec3(0.0f, 0.0f, float(unit) * cTrainUnitLength));
            else
                Park(train->mUnits[unit].mRoot);
        }

        SetMaterial(train->mLights, inMoving ? m_HeadlightMaterial : m_DarkLightMaterial);

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
            coin.mSpinPhase = ( m_Distance + inPosition.z ) * 0.3f;
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
        while (m_NextRowZ < cGenerateDistance)
        {
            GenerateRow(m_NextRowZ);
            m_NextRowZ += glm::clamp(m_Speed * 1.15f, 15.0f, 30.0f) + RandomFloat(0.0f, 6.0f);
        }
    }

    void UpdateSegments()
    {
        for (Segment& segment : m_Segments)
        {
            if (segment.mZ + m_TrackOffset + cSegmentLength < -20.0f)
            {
                segment.mZ += cSegmentCount * cSegmentLength;
                RandomizeSegment(segment);
            }

            SetTransform(segment.mRoot, Vec3(0.0f, 0.0f, segment.mZ + m_TrackOffset));
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
        const bool riding = m_SkateboardTimer > 0.0f;
        const float bob = running && m_Grounded && !sliding && !riding ? glm::abs(swing) * 0.08f : 0.0f;
        const float lean = riding ? glm::clamp(( GetLaneX(m_Lane) - m_PlayerX ) * 0.12f, -0.3f, 0.3f) : 0.0f;
        const float ride_height = riding ? cSkateboardRideHeight : 0.0f;

        float pitch = 0.0f;

        if (m_State == STATE_CRASHED || m_State == STATE_GAME_OVER)
            pitch = -glm::min(( m_Time - m_CrashTime ) * 6.0f, 1.45f);

        const bool flicker = m_InvulnerableTimer > 0.0f && glm::mod(m_Time, 0.16f) < 0.08f;
        const float body_yaw = riding && !sliding ? 0.95f : 0.0f;

        SetTransform(m_PlayerEntity, Vec3(m_PlayerX - bounce, m_PlayerY + bob + ride_height, 0.0f),Vec3(1.0f), Quat(Vec3(pitch, 0.0f, 0.0f)));
        SetTransform(m_BodyPivot, Vec3(0.0f, sliding ? 0.25f : 0.0f, 0.0f), Vec3(flicker ? 0.0f : 1.0f), Quat(Vec3(sliding ? -1.2f : 0.0f, body_yaw, -lean)));

        if (riding)
            SetTransform(m_Skateboard.mRoot, Vec3(0.0f, -cSkateboardRideHeight, 0.0f), Vec3(cSkateboardScale), Quat(Vec3(0.0f, 0.0f, -lean)));
        else
            Park(m_Skateboard.mRoot);

        for (int side = 0; side < 2; side++)
        {
            const float direction = side == 0 ? 1.0f : -1.0f;
            float leg = 0.0f;
            float arm = 0.0f;
            float arm_roll = side == 0 ? 0.12f : -0.12f;

            if (sliding)
            {
                leg = 1.1f;
                arm = -0.4f;
            }
            else if (running && riding)
            {
                leg = ( m_Grounded ? 0.3f : 0.7f ) * direction;
                arm = -0.25f + 0.1f * glm::sin(m_Time * 3.0f + direction);
                arm_roll = direction * 0.7f;
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

            SetTransform(m_Legs[side].mEntity, m_Legs[side].mRest, Vec3(1.0f), Quat(Vec3(leg, 0.0f, 0.0f)));
            SetTransform(m_Shoes[side].mEntity, m_Shoes[side].mRest, Vec3(1.0f), Quat(Vec3(leg, 0.0f, 0.0f)));
            SetTransform(m_Arms[side].mEntity, m_Arms[side].mRest, Vec3(1.0f), Quat(Vec3(arm, 0.0f, arm_roll)));
            SetMaterial(m_Shoes[side].mEntity, m_PowerUpTimers[POWER_UP_SNEAKERS] > 0.0f ? m_GlowingSneakerMaterial : m_PaletteMaterial);
        }

        UpdateGuard(inDeltaTime);

        for (const Obstacle& obstacle : m_Obstacles)
        {
            if (obstacle.mActive)
                SetTransform(obstacle.mEntity, Vec3(GetLaneX(obstacle.mLane), 0.0f, obstacle.mZ));
        }

        for (const Coin& coin : m_Coins)
        {
            if (coin.mActive)
                SetTransform(coin.mEntity, coin.mPosition, Vec3(1.0f), Quat(Vec3(0.0f, m_Time * 4.0f + coin.mSpinPhase, 0.0f)));
        }

        for (const PowerUpPickup& pickup : m_PowerUpPickups)
        {
            if (pickup.mActive)
                SetTransform(pickup.mEntity, pickup.mPosition + Vec3(0.0f, 0.15f * glm::sin(m_Time * 3.0f), 0.0f), Vec3(1.1f), Quat(Vec3(0.0f, m_Time * 2.0f, 0.0f)));
        }
    }

    void UpdateGuard(float inDeltaTime)
    {
        const bool caught = m_CaughtByGuard && ( m_State == STATE_CRASHED || m_State == STATE_GAME_OVER );
        const bool chasing = m_State == STATE_RUNNING && m_GuardTimer > cGuardDuration * 0.2f;

        const float target_z = caught ? -1.1f : chasing ? cGuardChaseZ : cGuardHiddenZ;
        const float approach = target_z > m_GuardPosition.z ? 4.0f : 1.2f;

        m_GuardPosition.z = glm::mix(m_GuardPosition.z, target_z, 1.0f - glm::exp(-inDeltaTime * approach));
        m_GuardPosition.x = glm::mix(m_GuardPosition.x, m_PlayerX + ( caught ? 0.0f : cGuardSideOffset ), 1.0f - glm::exp(-inDeltaTime * 4.0f));
        m_GuardPosition.y = glm::mix(m_GuardPosition.y, m_PlayerY, 1.0f - glm::exp(-inDeltaTime * 6.0f));

        if (m_GuardPosition.z < cGuardHiddenZ + 1.0f)
        {
            Park(m_Guard.mRoot);
            return;
        }

        if (m_State == STATE_RUNNING)
            m_GuardPhase += inDeltaTime * ( 6.0f + m_Speed * 0.35f );

        SetTransform(m_Guard.mRoot, m_GuardPosition);

        const float swing = m_State == STATE_RUNNING ? glm::sin(m_GuardPhase) : 0.0f;

        for (int side = 0; side < 2; side++)
        {
            const float direction = side == 0 ? 1.0f : -1.0f;
            const ModelPart leg = FindPart(m_Guard, side == 0 ? "LegL" : "LegR");
            const ModelPart arm = FindPart(m_Guard, side == 0 ? "ArmL" : "ArmR");

            const float arm_angle = caught ? -1.5f : -swing * 0.9f * direction;

            SetTransform(leg.mEntity, leg.mRest, Vec3(1.0f), Quat(Vec3(swing * 0.8f * direction, 0.0f, 0.0f)));
            SetTransform(arm.mEntity, arm.mRest, Vec3(1.0f), Quat(Vec3(arm_angle, 0.0f, side == 0 ? 0.1f : -0.1f)));
        }
    }

    void UpdateCamera(float inDeltaTime)
    {
        Transform* transform = FindComponent<Transform>(m_CameraEntity);

        if (transform == nullptr)
            return;

        const float smoothing = 1.0f - glm::exp(-inDeltaTime * 8.0f);

        Vec3 target_position = Vec3(m_PlayerX * 0.75f, m_PlayerY * 0.5f + 5.6f, -6.2f);

        if (m_State == STATE_READY)
            target_position = Vec3(m_PlayerX + 2.5f * glm::sin(m_Time * 0.5f), 2.4f, -5.5f);

        m_CameraPosition.x = glm::mix(m_CameraPosition.x, target_position.x, smoothing);
        m_CameraPosition.y = glm::mix(m_CameraPosition.y, target_position.y, 1.0f - glm::exp(-inDeltaTime * 5.0f));
        m_CameraPosition.z = m_State == STATE_RUNNING ? target_position.z : glm::mix(m_CameraPosition.z, target_position.z, smoothing);
        m_CameraLookY = glm::mix(m_CameraLookY, m_State == STATE_READY ? 1.2f : m_PlayerY * 0.5f + 0.4f, 1.0f - glm::exp(-inDeltaTime * 5.0f));

        m_Shake = glm::max(0.0f, m_Shake - inDeltaTime * 1.5f);
        const Vec3 shake = Vec3(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f), 0.0f) * m_Shake * m_Shake * 0.5f;

        const Vec3 position = m_CameraPosition + shake;
        const Vec3 look_at = Vec3(m_CameraPosition.x * 0.8f, m_CameraLookY, m_State == STATE_READY ? 6.0f : 4.5f);

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

        g_UIRenderer.AddText(Vec2(margin, margin), std::format("{} m", int(m_Distance)), 30.0f * scale, Vec4(1.0f, 1.0f, 1.0f, 0.9f));

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

        if (m_SkateboardTimer > 0.0f)
        {
            const Vec2 pos = Vec2(margin, bar_y);
            const Vec2 size = Vec2(240.0f, 22.0f) * scale;

            g_UIRenderer.AddRectFilled(pos, size, size.y * 0.5f, Vec4(0.0f, 0.0f, 0.0f, 0.55f));
            g_UIRenderer.AddRectFilled(pos, Vec2(glm::max(size.x * m_SkateboardTimer / cSkateboardDuration, size.y), size.y), size.y * 0.5f, Vec4(cSkateboardColor, 1.0f));
            g_UIRenderer.AddText(pos + Vec2(10.0f, 2.0f) * scale, "SKATEBOARD", 15.0f * scale, Vec4(0.05f, 0.05f, 0.08f, 1.0f));
        }

        const Vec4 board_color = m_SkateboardsLeft > 0 ? Vec4(cSkateboardColor, 1.0f) : Vec4(1.0f, 1.0f, 1.0f, 0.35f);
        DrawOutlinedText(score_pos + Vec2(260.0f, 132.0f) * scale, std::format("[E] SKATEBOARD x{}", m_SkateboardsLeft), 18.0f * scale, board_color, UI_TEXT_ALIGN_RIGHT, scale);

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

        static constexpr std::array<std::pair<const char*, const char*>, 6> cControls =
        {
            std::pair { "A / D",           "Switch lanes" },
            std::pair { "W / SPACE",       "Jump" },
            std::pair { "S",               "Slide, or slam down mid-air" },
            std::pair { "E",               "Skateboard, survives one crash" },
            std::pair { "G / F",           "Cheats: god mode / speed" },
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
        g_UIRenderer.AddText(Vec2(center, pos.y + 170.0f * inScale), std::format("{} m   {} coins   best {}", int(m_Distance), m_CoinCount, m_BestScore), 22.0f * inScale, Vec4(1.0f, 0.85f, 0.5f, alpha), UI_TEXT_ALIGN_CENTER);

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
    Model m_Runner;
    StaticArray<ModelPart, 2> m_Legs;
    StaticArray<ModelPart, 2> m_Arms;
    StaticArray<ModelPart, 2> m_Shoes;

    Model m_Guard;
    Vec3 m_GuardPosition = Vec3(0.0f, 0.0f, cGuardHiddenZ);
    float m_GuardPhase = 0.0f;
    bool m_CaughtByGuard = false;

    Model m_Skateboard;
    float m_SkateboardTimer = 0.0f;
    float m_InvulnerableTimer = 0.0f;
    int m_SkateboardsLeft = cSkateboardsPerRun;

    HashMap<String, Array<PartTemplate>> m_MeshTemplates;
    uint32_t m_PaletteTexture = 0;
    uint32_t m_PaletteEmissiveTexture = 0;

    Entity m_PaletteMaterial = Entity::Null;
    Entity m_HeadlightMaterial = Entity::Null;
    Entity m_DarkLightMaterial = Entity::Null;
    Entity m_GlowingSneakerMaterial = Entity::Null;

    StaticArray<Segment, cSegmentCount> m_Segments;
    Array<Obstacle> m_Obstacles;
    Array<Coin> m_Coins;
    StaticArray<PowerUpPickup, POWER_UP_COUNT> m_PowerUpPickups;

    float m_Speed = cStartSpeed;
    float m_Distance = 0.0f;
    float m_TrackOffset = 0.0f;
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
    RailRushScript() = default;
};


RTTI_DEFINE_TYPE(RailRushScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(RailRushScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(RailRushScript, SERIALIZE_ALL, "Best Score", m_BestScore);
}

RK_REGISTER_SCRIPT(RailRushScript)

} // RK
