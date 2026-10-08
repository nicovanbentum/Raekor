#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"

namespace RK {

static constexpr float cArenaHalfSize = 40.0f;
static constexpr float cPlayerRadius = 0.45f;
static constexpr float cPlayerBaseSpeed = 6.0f;
static constexpr float cPlayerBaseHealth = 100.0f;
static constexpr float cPlayerInvulnerability = 0.6f;
static constexpr float cBaseMagnetRadius = 2.5f;
static constexpr float cSpawnDistance = 26.0f;
static constexpr float cMinSpawnDistance = 15.0f;
static constexpr float cDeathDuration = 0.18f;
static constexpr float cFlashDuration = 0.08f;
static constexpr float cVictoryTime = 600.0f;
static constexpr float cSwarmInterval = 90.0f;
static constexpr float cBruteInterval = 60.0f;

static constexpr float cGridCellSize = 2.0f;
static constexpr int   cGridDim = int(( cArenaHalfSize * 2.0f + 4.0f ) / cGridCellSize) + 1;

static constexpr int cProjectilePoolSize = 80;
static constexpr int cGemPoolSize = 250;
static constexpr int cMaxBlades = 6;
static constexpr int cMaxUpgradeLevel = 5;
static constexpr int cMaxDamageNumbers = 64;
static constexpr int cChoiceCount = 3;

static constexpr Vec3 cParkedPosition = Vec3(0.0f, -60.0f, 0.0f);

static constexpr const char* cRuntimeRootName = "DuskSwarm Runtime";


enum EEnemyType
{
    ENEMY_GHOUL,
    ENEMY_BAT,
    ENEMY_BRUTE,
    ENEMY_TYPE_COUNT
};


struct EnemyArchetype
{
    const char* mName;
    int mPoolSize;
    float mRadius;
    float mSpeed;
    float mHealth;
    float mDamage;
    int mGemValue;
    float mHeight;
    bool mIsSphere;
    Vec3 mScale;
    Vec3 mColor;
};


static const StaticArray<EnemyArchetype, ENEMY_TYPE_COUNT> cEnemyArchetypes =
{
    EnemyArchetype { "Ghoul", 150, 0.45f, 2.6f, 4.0f,  8.0f,  1, 0.55f, false, Vec3(0.75f, 1.1f, 0.6f), Vec3(0.30f, 0.42f, 0.28f) },
    EnemyArchetype { "Bat",   80,  0.35f, 4.4f, 2.0f,  5.0f,  1, 1.10f, true,  Vec3(1.0f),             Vec3(0.42f, 0.22f, 0.55f) },
    EnemyArchetype { "Brute", 12,  0.95f, 1.8f, 60.0f, 14.0f, 5, 1.00f, false, Vec3(1.7f, 2.0f, 1.4f), Vec3(0.55f, 0.12f, 0.10f) },
};


enum EUpgrade
{
    UPGRADE_BOLT,
    UPGRADE_BLADES,
    UPGRADE_NOVA,
    UPGRADE_BOOTS,
    UPGRADE_VITALITY,
    UPGRADE_MAGNET,
    UPGRADE_REGEN,
    UPGRADE_POWER,
    UPGRADE_HASTE,
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
    UpgradeInfo { "Arcane Bolt",    "Fires homing bolts at the nearest foe",       true,  Vec3(0.35f, 0.85f, 1.0f) },
    UpgradeInfo { "Orbiting Blades", "Blades that circle you and cut through foes", true,  Vec3(1.0f, 0.60f, 0.20f) },
    UpgradeInfo { "Nova",           "Periodic shockwave that knocks foes back",    true,  Vec3(0.75f, 0.45f, 1.0f) },
    UpgradeInfo { "Swift Boots",    "+12% movement speed",                         false, Vec3(0.45f, 1.0f, 0.60f) },
    UpgradeInfo { "Vitality",       "+25 max health and heal 25",                  false, Vec3(1.0f, 0.35f, 0.35f) },
    UpgradeInfo { "Magnet",         "+40% pickup range",                           false, Vec3(0.35f, 1.0f, 0.85f) },
    UpgradeInfo { "Regeneration",   "+0.6 health per second",                      false, Vec3(1.0f, 0.55f, 0.65f) },
    UpgradeInfo { "Power",          "+15% damage for all weapons",                 false, Vec3(1.0f, 0.85f, 0.35f) },
    UpgradeInfo { "Haste",          "-8% weapon cooldowns",                        false, Vec3(0.55f, 0.75f, 1.0f) },
    UpgradeInfo { "Feast",          "Heal 30% of your max health",                 false, Vec3(1.0f, 0.75f, 0.45f) },
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

    struct Enemy
    {
        Entity mEntity = Entity::Null;
        int mType = ENEMY_GHOUL;
        bool mActive = false;
        bool mDying = false;
        Vec2 mPosition = Vec2(0.0f);
        Vec2 mKnockback = Vec2(0.0f);
        float mHealth = 0.0f;
        float mFlashTimer = 0.0f;
        float mDeathTimer = 0.0f;
        float mBladeCooldown = 0.0f;
        float mPhase = 0.0f;
        float mYaw = 0.0f;
    };

    struct Projectile
    {
        Entity mEntity = Entity::Null;
        bool mActive = false;
        Vec2 mPosition = Vec2(0.0f);
        Vec2 mVelocity = Vec2(0.0f);
        float mLifetime = 0.0f;
        float mDamage = 0.0f;
        int mPierce = 0;
        int mLastHit = -1;
    };

    struct Gem
    {
        Entity mEntity = Entity::Null;
        bool mActive = false;
        bool mAttracted = false;
        Vec2 mPosition = Vec2(0.0f);
        float mSpeed = 0.0f;
        float mPhase = 0.0f;
        int mValue = 1;
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

        CreateMaterials();
        CreateArena();
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

        if (m_State == STATE_PLAYING)
        {
            m_RunTime += inDeltaTime;

            UpdatePlayer(inDeltaTime);
            UpdateSpawning(inDeltaTime);
            BuildGrid();
            UpdateEnemies(inDeltaTime);
            UpdateWeapons(inDeltaTime);
            UpdateProjectiles(inDeltaTime);
            UpdateGems(inDeltaTime);
            UpdateProgress();
        }

        UpdateDying(inDeltaTime);
        UpdateVisuals(inDeltaTime);
        UpdateCamera(inDeltaTime);
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
            if (key == SDLK_RETURN)
                NewGame();

            return;
        }

        if (m_State != STATE_LEVEL_UP || m_ChoiceCount == 0)
            return;

        switch (key)
        {
            case SDLK_1: ChooseUpgrade(0); break;
            case SDLK_2: ChooseUpgrade(1); break;
            case SDLK_3: ChooseUpgrade(2); break;
            case SDLK_LEFT:
            case SDLK_A: m_SelectedChoice = ( m_SelectedChoice + m_ChoiceCount - 1 ) % m_ChoiceCount; break;
            case SDLK_RIGHT:
            case SDLK_D: m_SelectedChoice = ( m_SelectedChoice + 1 ) % m_ChoiceCount; break;
            case SDLK_SPACE:
            case SDLK_RETURN: ChooseUpgrade(m_SelectedChoice); break;
        }
    }

private:
    Entity CreateEntity(StringView inName, Entity inParent = Entity::Null)
    {
        const Entity entity = m_Scene->CreateSpatialEntity(inName);
        m_Scene->ParentTo(entity, inParent != Entity::Null ? inParent : m_RootEntity);
        return entity;
    }

    Entity CreateMaterial(Vec3 inAlbedo, Vec3 inEmissive, float inRoughness, float inMetallic = 0.0f, float inAlpha = 1.0f)
    {
        const Entity entity = CreateEntity("Material");

        Material& material = m_Scene->Add<Material>(entity);
        material.albedo = Vec4(inAlbedo, inAlpha);
        material.emissive = inEmissive;
        material.roughness = inRoughness;
        material.metallic = inMetallic;
        material.blendMode = inAlpha < 1.0f ? MATERIAL_BLEND_MODE_BLENDED : MATERIAL_BLEND_MODE_OPAQUE;

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

    void SetTransform(Entity inEntity, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation = Quat(Vec3(0.0f)))
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

    static Vec3 ToWorld(const Vec2& inPosition, float inHeight)
    {
        return Vec3(inPosition.x, inHeight, inPosition.y);
    }

    static float GetYaw(const Vec2& inDirection)
    {
        return glm::atan(-inDirection.x, -inDirection.y);
    }

    static Vec2 ClampToArena(const Vec2& inPosition, float inRadius)
    {
        return glm::clamp(inPosition, Vec2(-cArenaHalfSize + inRadius), Vec2(cArenaHalfSize - inRadius));
    }

    void CreateMaterials()
    {
        for (int type = 0; type < ENEMY_TYPE_COUNT; type++)
            m_EnemyMaterials[type] = CreateMaterial(cEnemyArchetypes[type].mColor, Vec3(0.0f), 0.7f);

        m_FlashMaterial = CreateMaterial(Vec3(1.0f), Vec3(1.0f, 0.95f, 0.9f) * 40000.0f, 0.3f);
        m_GroundMaterial = CreateMaterial(Vec3(0.16f, 0.20f, 0.12f), Vec3(0.0f), 0.95f);
        m_DirtMaterial = CreateMaterial(Vec3(0.20f, 0.15f, 0.10f), Vec3(0.0f), 1.0f);
        m_StoneMaterial = CreateMaterial(Vec3(0.42f, 0.44f, 0.48f), Vec3(0.0f), 0.8f);
        m_WallMaterial = CreateMaterial(Vec3(0.30f, 0.29f, 0.30f), Vec3(0.0f), 0.9f);
        m_WoodMaterial = CreateMaterial(Vec3(0.18f, 0.12f, 0.08f), Vec3(0.0f), 0.9f);
        m_LanternMaterial = CreateMaterial(Vec3(1.0f, 0.6f, 0.25f), Vec3(1.0f, 0.55f, 0.2f) * 30000.0f, 0.4f);
        m_PlayerBodyMaterial = CreateMaterial(Vec3(0.12f, 0.18f, 0.40f), Vec3(0.0f), 0.5f);
        m_PlayerHeadMaterial = CreateMaterial(Vec3(0.85f, 0.70f, 0.58f), Vec3(0.0f), 0.6f);
        m_VisorMaterial = CreateMaterial(Vec3(0.35f, 0.85f, 1.0f), Vec3(0.35f, 0.85f, 1.0f) * 20000.0f, 0.2f);
        m_BoltMaterial = CreateMaterial(Vec3(0.35f, 0.85f, 1.0f), Vec3(0.35f, 0.85f, 1.0f) * 25000.0f, 0.2f);
        m_BladeMaterial = CreateMaterial(Vec3(1.0f, 0.6f, 0.2f), Vec3(1.0f, 0.55f, 0.15f) * 20000.0f, 0.2f, 0.8f);
        m_NovaMaterial = CreateMaterial(Vec3(0.75f, 0.45f, 1.0f), Vec3(0.75f, 0.45f, 1.0f) * 6000.0f, 0.2f, 0.0f, 0.4f);
        m_SmallGemMaterial = CreateMaterial(Vec3(0.3f, 1.0f, 0.5f), Vec3(0.3f, 1.0f, 0.5f) * 8000.0f, 0.2f);
        m_LargeGemMaterial = CreateMaterial(Vec3(0.3f, 0.6f, 1.0f), Vec3(0.3f, 0.6f, 1.0f) * 12000.0f, 0.2f);
    }

    void CreateArena()
    {
        const float ground_size = cArenaHalfSize * 2.0f + 10.0f;
        CreateShape("Ground", false, m_GroundMaterial, Vec3(0.0f, -0.5f, 0.0f), Vec3(ground_size, 1.0f, ground_size));

        for (int index = 0; index < 30; index++)
        {
            const Vec2 position = Vec2(RandomFloat(-cArenaHalfSize, cArenaHalfSize), RandomFloat(-cArenaHalfSize, cArenaHalfSize));
            const Vec3 scale = Vec3(RandomFloat(2.0f, 6.0f), 0.02f, RandomFloat(2.0f, 6.0f));
            CreateShape("Dirt", true, m_DirtMaterial, ToWorld(position, 0.0f), scale, Entity::Null, Quat(Vec3(0.0f, RandomFloat(0.0f, glm::pi<float>()), 0.0f)));
        }

        const float wall_height = 0.9f;
        const float wall_length = cArenaHalfSize * 2.0f + 1.0f;

        CreateShape("Wall", false, m_WallMaterial, Vec3(0.0f, wall_height * 0.5f, -cArenaHalfSize - 0.5f), Vec3(wall_length, wall_height, 1.0f));
        CreateShape("Wall", false, m_WallMaterial, Vec3(0.0f, wall_height * 0.5f, cArenaHalfSize + 0.5f), Vec3(wall_length, wall_height, 1.0f));
        CreateShape("Wall", false, m_WallMaterial, Vec3(-cArenaHalfSize - 0.5f, wall_height * 0.5f, 0.0f), Vec3(1.0f, wall_height, wall_length));
        CreateShape("Wall", false, m_WallMaterial, Vec3(cArenaHalfSize + 0.5f, wall_height * 0.5f, 0.0f), Vec3(1.0f, wall_height, wall_length));

        for (float offset = -cArenaHalfSize; offset <= cArenaHalfSize; offset += 10.0f)
        {
            for (const Vec2& position : { Vec2(offset, -cArenaHalfSize - 0.5f), Vec2(offset, cArenaHalfSize + 0.5f), Vec2(-cArenaHalfSize - 0.5f, offset), Vec2(cArenaHalfSize + 0.5f, offset) })
            {
                CreateShape("Lantern Post", false, m_WoodMaterial, ToWorld(position, 1.1f), Vec3(0.25f, 2.2f, 0.25f));
                CreateShape("Lantern", false, m_LanternMaterial, ToWorld(position, 2.3f), Vec3(0.4f));
            }
        }

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

        Vec2 position;

        for (int index = 0; index < 30; index++)
        {
            if (PlaceObstacle(0.45f, position))
                CreateShape("Gravestone", false, m_StoneMaterial, ToWorld(position, 0.5f), Vec3(0.7f, 1.0f, 0.25f), Entity::Null, Quat(Vec3(RandomFloat(-0.12f, 0.12f), RandomFloat(-0.4f, 0.4f), RandomFloat(-0.1f, 0.1f))));
        }

        for (int index = 0; index < 18; index++)
        {
            if (PlaceObstacle(0.75f, position))
                CreateShape("Rock", true, m_StoneMaterial, ToWorld(position, 0.2f), Vec3(RandomFloat(1.2f, 1.8f), RandomFloat(0.7f, 1.1f), RandomFloat(1.2f, 1.8f)), Entity::Null, Quat(Vec3(0.0f, RandomFloat(0.0f, glm::pi<float>()), 0.0f)));
        }

        for (int index = 0; index < 12; index++)
        {
            if (!PlaceObstacle(0.4f, position))
                continue;

            const float height = RandomFloat(2.6f, 3.6f);
            const float yaw = RandomFloat(0.0f, glm::two_pi<float>());

            const Entity tree = CreateEntity("Dead Tree");
            SetTransform(tree, ToWorld(position, 0.0f), Vec3(1.0f), Quat(Vec3(0.0f, yaw, 0.0f)));

            CreateShape("Trunk", false, m_WoodMaterial, Vec3(0.0f, height * 0.5f, 0.0f), Vec3(0.35f, height, 0.35f), tree);
            CreateShape("Branch", false, m_WoodMaterial, Vec3(0.45f, height * 0.7f, 0.0f), Vec3(1.1f, 0.16f, 0.16f), tree, Quat(Vec3(0.0f, 0.0f, 0.6f)));
            CreateShape("Branch", false, m_WoodMaterial, Vec3(-0.35f, height * 0.85f, 0.1f), Vec3(0.9f, 0.14f, 0.14f), tree, Quat(Vec3(0.0f, 0.3f, -0.7f)));
        }
    }

    void CreatePlayer()
    {
        m_PlayerEntity = CreateEntity("Player");

        CreateShape("Body", false, m_PlayerBodyMaterial, Vec3(0.0f, 0.6f, 0.0f), Vec3(0.7f, 1.0f, 0.5f), m_PlayerEntity);
        m_PlayerHeadEntity = CreateShape("Head", true, m_PlayerHeadMaterial, Vec3(0.0f, 1.38f, 0.0f), Vec3(0.55f), m_PlayerEntity);
        CreateShape("Visor", false, m_VisorMaterial, Vec3(0.0f, 1.42f, -0.24f), Vec3(0.36f, 0.1f, 0.12f), m_PlayerEntity);

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
            m_FreeEnemies[type].clear();

            for (int index = 0; index < archetype.mPoolSize; index++)
            {
                Enemy& enemy = m_Enemies.emplace_back();
                enemy.mType = type;
                enemy.mEntity = CreateShape(archetype.mName, archetype.mIsSphere, m_EnemyMaterials[type], cParkedPosition, Vec3(0.1f));
            }
        }

        m_Projectiles.resize(cProjectilePoolSize);
        for (Projectile& projectile : m_Projectiles)
            projectile.mEntity = CreateShape("Bolt", true, m_BoltMaterial, cParkedPosition, Vec3(0.1f));

        m_Gems.resize(cGemPoolSize);
        for (Gem& gem : m_Gems)
            gem.mEntity = CreateShape("Gem", false, m_SmallGemMaterial, cParkedPosition, Vec3(0.1f));

        for (Entity& blade : m_Blades)
            blade = CreateShape("Blade", true, m_BladeMaterial, cParkedPosition, Vec3(0.1f));

        m_NovaEntity = CreateShape("Nova", true, m_NovaMaterial, cParkedPosition, Vec3(0.1f));
    }

    void CreateCamera()
    {
        m_CameraEntity = CreateEntity("Swarm Camera");

        Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
        camera.SetFov(50.0f);

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
        m_PlayerHeadEntity = Entity::Null;
        m_NovaEntity = Entity::Null;
        m_Blades.fill(Entity::Null);
        m_Enemies.clear();
        m_Projectiles.clear();
        m_Gems.clear();
        m_Obstacles.clear();

        for (Array<int>& free_list : m_FreeEnemies)
            free_list.clear();
    }

    void NewGame()
    {
        for (Enemy& enemy : m_Enemies)
        {
            enemy.mActive = false;
            enemy.mDying = false;
            SetMaterial(enemy.mEntity, m_EnemyMaterials[enemy.mType]);
            Park(enemy.mEntity);
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

        for (Projectile& projectile : m_Projectiles)
        {
            projectile.mActive = false;
            Park(projectile.mEntity);
        }

        for (Gem& gem : m_Gems)
        {
            gem.mActive = false;
            Park(gem.mEntity);
        }

        for (Entity blade : m_Blades)
            Park(blade);

        Park(m_NovaEntity);

        m_UpgradeLevels.fill(0);
        m_UpgradeLevels[UPGRADE_BOLT] = 1;

        m_State = STATE_PLAYING;
        m_RunTime = 0.0f;
        m_PlayerPosition = Vec2(0.0f);
        m_PlayerVelocity = Vec2(0.0f);
        m_PlayerYaw = 0.0f;
        m_Health = cPlayerBaseHealth;
        m_InvulnerableTimer = 0.0f;
        m_HitFlash = 0.0f;
        m_Shake = 0.0f;
        m_Level = 1;
        m_Experience = 0;
        m_PendingLevelUps = 0;
        m_Kills = 0;
        m_SpawnAccumulator = 0.0f;
        m_NextSwarmTime = cSwarmInterval;
        m_NextBruteTime = cBruteInterval;
        m_BoltTimer = 0.5f;
        m_NovaTimer = 2.0f;
        m_NovaAge = -1.0f;
        m_BladeAngle = 0.0f;
        m_EndTime = 0.0f;
        m_ChoiceCount = 0;
        m_DamageNumbers.clear();
        m_Popups.clear();
        m_CameraTarget = Vec3(0.0f);
    }

    float GetMaxHealth() const { return cPlayerBaseHealth + 25.0f * m_UpgradeLevels[UPGRADE_VITALITY]; }
    float GetMoveSpeed() const { return cPlayerBaseSpeed * ( 1.0f + 0.12f * m_UpgradeLevels[UPGRADE_BOOTS] ); }
    float GetMagnetRadius() const { return cBaseMagnetRadius * ( 1.0f + 0.4f * m_UpgradeLevels[UPGRADE_MAGNET] ); }
    float GetDamageMultiplier() const { return 1.0f + 0.15f * m_UpgradeLevels[UPGRADE_POWER]; }
    float GetCooldownMultiplier() const { return glm::pow(0.92f, float(m_UpgradeLevels[UPGRADE_HASTE])); }
    int   GetExperienceToNextLevel() const { return 5 + ( m_Level - 1 ) * 4 + ( m_Level - 1 ) * ( m_Level - 1 ) / 3; }

    void UpdatePlayer(float inDeltaTime)
    {
        Vec2 input = Vec2(0.0f);

        if (m_Input->IsKeyDown(Key::W) || m_Input->IsKeyDown(Key::UP))    input.y -= 1.0f;
        if (m_Input->IsKeyDown(Key::S) || m_Input->IsKeyDown(Key::DOWN))  input.y += 1.0f;
        if (m_Input->IsKeyDown(Key::A) || m_Input->IsKeyDown(Key::LEFT))  input.x -= 1.0f;
        if (m_Input->IsKeyDown(Key::D) || m_Input->IsKeyDown(Key::RIGHT)) input.x += 1.0f;

        if (glm::length(input) > 0.0f)
            input = glm::normalize(input);

        const Vec2 target_velocity = input * GetMoveSpeed();
        m_PlayerVelocity = glm::mix(m_PlayerVelocity, target_velocity, 1.0f - glm::exp(-inDeltaTime * 18.0f));

        m_PlayerPosition += m_PlayerVelocity * inDeltaTime;
        m_PlayerPosition = ResolveObstacles(m_PlayerPosition, cPlayerRadius);
        m_PlayerPosition = ClampToArena(m_PlayerPosition, cPlayerRadius);

        if (glm::length(input) > 0.0f)
            m_FacingDirection = input;

        const float max_health = GetMaxHealth();
        m_Health = glm::min(max_health, m_Health + 0.6f * m_UpgradeLevels[UPGRADE_REGEN] * inDeltaTime);

        m_InvulnerableTimer = glm::max(0.0f, m_InvulnerableTimer - inDeltaTime);
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
        if (m_InvulnerableTimer > 0.0f || m_State != STATE_PLAYING)
            return;

        m_Health -= inDamage;
        m_InvulnerableTimer = cPlayerInvulnerability;
        m_HitFlash = 1.0f;
        m_Shake = glm::max(m_Shake, 0.35f);

        if (m_Health <= 0.0f)
        {
            m_Health = 0.0f;
            m_State = STATE_GAME_OVER;
            m_EndTime = m_Time;
            m_BestTime = glm::max(m_BestTime, m_RunTime);
        }
    }

    void UpdateSpawning(float inDeltaTime)
    {
        const float minutes = m_RunTime / 60.0f;
        const float spawn_rate = 1.2f + minutes * 1.6f + minutes * minutes * 0.25f;

        m_SpawnAccumulator += spawn_rate * inDeltaTime;

        while (m_SpawnAccumulator >= 1.0f)
        {
            m_SpawnAccumulator -= 1.0f;

            const float bat_chance = glm::clamp(( m_RunTime - 45.0f ) / 240.0f, 0.0f, 0.4f);
            SpawnEnemy(RandomFloat(0.0f, 1.0f) < bat_chance ? ENEMY_BAT : ENEMY_GHOUL, GetSpawnPosition());
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

            AddPopup("THE SWARM DESCENDS", Vec4(0.8f, 0.55f, 1.0f, 1.0f));
        }

        if (m_RunTime >= m_NextBruteTime)
        {
            m_NextBruteTime += cBruteInterval;

            const int count = 1 + int(minutes / 3.0f);

            for (int index = 0; index < count; index++)
                SpawnEnemy(ENEMY_BRUTE, GetSpawnPosition());

            AddPopup(count > 1 ? "BRUTES APPROACH" : "A BRUTE APPROACHES", Vec4(1.0f, 0.45f, 0.35f, 1.0f));
        }
    }

    Vec2 GetSpawnPosition()
    {
        Vec2 best = ClampToArena(m_PlayerPosition + Vec2(cSpawnDistance, 0.0f), 1.0f);
        float best_distance = 0.0f;

        for (int attempt = 0; attempt < 8; attempt++)
        {
            const float angle = RandomFloat(0.0f, glm::two_pi<float>());
            const Vec2 position = ClampToArena(m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * RandomFloat(cSpawnDistance, cSpawnDistance + 4.0f), 1.0f);
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

    void SpawnEnemy(int inType, const Vec2& inPosition)
    {
        Array<int>& free_list = m_FreeEnemies[inType];

        if (free_list.empty())
            return;

        Enemy& enemy = m_Enemies[free_list.back()];
        free_list.pop_back();

        const float health_scale = 1.0f + m_RunTime / 150.0f;

        enemy.mActive = true;
        enemy.mDying = false;
        enemy.mPosition = inPosition;
        enemy.mKnockback = Vec2(0.0f);
        enemy.mHealth = cEnemyArchetypes[inType].mHealth * health_scale;
        enemy.mFlashTimer = 0.0f;
        enemy.mDeathTimer = 0.0f;
        enemy.mBladeCooldown = 0.0f;
        enemy.mPhase = RandomFloat(0.0f, glm::two_pi<float>());
        enemy.mYaw = GetYaw(m_PlayerPosition - inPosition);

        SetMaterial(enemy.mEntity, m_EnemyMaterials[inType]);
    }

    void BuildGrid()
    {
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
        const IVec2 min_coord = GetGridCoord(inPosition - Vec2(inRadius));
        const IVec2 max_coord = GetGridCoord(inPosition + Vec2(inRadius));

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

    void UpdateEnemies(float inDeltaTime)
    {
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
                const float min_distance = radius + cEnemyArchetypes[inOtherEnemy.mType].mRadius;

                if (distance < min_distance)
                {
                    const Vec2 direction = distance > 0.0001f ? delta / distance : Vec2(glm::cos(enemy.mPhase), glm::sin(enemy.mPhase));
                    const float weight = cEnemyArchetypes[inOtherEnemy.mType].mRadius / ( radius + cEnemyArchetypes[inOtherEnemy.mType].mRadius );
                    m_Pushes[index] += direction * ( min_distance - distance ) * weight;
                }
            });
        }

        for (size_t index = 0; index < m_Enemies.size(); index++)
        {
            Enemy& enemy = m_Enemies[index];

            if (!enemy.mActive || enemy.mDying)
                continue;

            const EnemyArchetype& archetype = cEnemyArchetypes[enemy.mType];

            Vec2 to_player = m_PlayerPosition - enemy.mPosition;
            const float distance = glm::length(to_player);
            to_player = distance > 0.0001f ? to_player / distance : Vec2(0.0f);

            Vec2 velocity = to_player * archetype.mSpeed;

            if (enemy.mType == ENEMY_BAT)
                velocity += Vec2(-to_player.y, to_player.x) * glm::sin(m_RunTime * 3.0f + enemy.mPhase) * 1.5f;

            enemy.mPosition += ( velocity + enemy.mKnockback ) * inDeltaTime + m_Pushes[index] * 0.5f;
            enemy.mKnockback *= glm::exp(-inDeltaTime * 8.0f);

            if (enemy.mType != ENEMY_BAT)
                enemy.mPosition = ResolveObstacles(enemy.mPosition, archetype.mRadius);

            enemy.mPosition = ClampToArena(enemy.mPosition, archetype.mRadius);

            if (distance > 0.0001f)
                enemy.mYaw = GetYaw(to_player);

            enemy.mFlashTimer = glm::max(0.0f, enemy.mFlashTimer - inDeltaTime);
            enemy.mBladeCooldown = glm::max(0.0f, enemy.mBladeCooldown - inDeltaTime);

            if (enemy.mFlashTimer <= 0.0f)
                SetMaterial(enemy.mEntity, m_EnemyMaterials[enemy.mType]);

            if (glm::distance(enemy.mPosition, m_PlayerPosition) < archetype.mRadius + cPlayerRadius)
                DamagePlayer(archetype.mDamage);

            if (distance > 60.0f)
                Despawn(int(index));
        }
    }

    void Despawn(int inIndex)
    {
        Enemy& enemy = m_Enemies[inIndex];
        enemy.mActive = false;
        enemy.mDying = false;
        Park(enemy.mEntity);
        m_FreeEnemies[enemy.mType].push_back(inIndex);
    }

    void DamageEnemy(int inIndex, float inDamage, const Vec2& inKnockback)
    {
        Enemy& enemy = m_Enemies[inIndex];

        if (!enemy.mActive || enemy.mDying)
            return;

        const float knockback_scale = enemy.mType == ENEMY_BRUTE ? 0.15f : 1.0f;

        enemy.mHealth -= inDamage;
        enemy.mKnockback += inKnockback * knockback_scale;
        enemy.mFlashTimer = cFlashDuration;
        SetMaterial(enemy.mEntity, m_FlashMaterial);

        AddDamageNumber(ToWorld(enemy.mPosition, cEnemyArchetypes[enemy.mType].mHeight + 0.8f), int(glm::ceil(inDamage)));

        if (enemy.mHealth <= 0.0f)
        {
            enemy.mDying = true;
            enemy.mDeathTimer = 0.0f;
            m_Kills++;

            const int value = cEnemyArchetypes[enemy.mType].mGemValue;

            if (enemy.mType == ENEMY_BRUTE)
            {
                for (int gem = 0; gem < 4; gem++)
                    SpawnGem(enemy.mPosition + Vec2(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f)), value);
            }
            else
            {
                SpawnGem(enemy.mPosition, value);
            }
        }
    }

    void UpdateDying(float inDeltaTime)
    {
        for (size_t index = 0; index < m_Enemies.size(); index++)
        {
            Enemy& enemy = m_Enemies[index];

            if (!enemy.mActive || !enemy.mDying)
                continue;

            enemy.mDeathTimer += inDeltaTime;

            if (enemy.mDeathTimer >= cDeathDuration)
                Despawn(int(index));
        }
    }

    void UpdateWeapons(float inDeltaTime)
    {
        const float damage_multiplier = GetDamageMultiplier();
        const float cooldown_multiplier = GetCooldownMultiplier();

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
            const float radius = 2.0f + 0.2f * level;
            const float damage = ( 2.0f + 0.75f * level ) * damage_multiplier;

            for (int blade = 0; blade < count; blade++)
            {
                const float angle = m_BladeAngle + glm::two_pi<float>() * float(blade) / float(count);
                const Vec2 position = m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * radius;

                ForEachEnemyNear(position, 0.35f, [&](int inIndex, Enemy& inEnemy)
                {
                    if (inEnemy.mBladeCooldown > 0.0f)
                        return;

                    inEnemy.mBladeCooldown = 0.45f;

                    const Vec2 away = inEnemy.mPosition - m_PlayerPosition;
                    DamageEnemy(inIndex, damage, glm::length(away) > 0.0001f ? glm::normalize(away) * 6.0f : Vec2(0.0f));
                });
            }
        }

        if (const int level = m_UpgradeLevels[UPGRADE_NOVA]; level > 0)
        {
            m_NovaTimer -= inDeltaTime;

            if (m_NovaTimer <= 0.0f)
            {
                m_NovaTimer = ( 3.6f - 0.35f * ( level - 1 ) ) * cooldown_multiplier;
                m_NovaAge = 0.0f;
                m_NovaRadius = 3.5f + 0.6f * level;

                const float damage = ( 4.0f + 1.5f * level ) * damage_multiplier;

                ForEachEnemyNear(m_PlayerPosition, m_NovaRadius, [&](int inIndex, Enemy& inEnemy)
                {
                    const Vec2 away = inEnemy.mPosition - m_PlayerPosition;
                    DamageEnemy(inIndex, damage, glm::length(away) > 0.0001f ? glm::normalize(away) * 14.0f : Vec2(0.0f));
                });

                m_Shake = glm::max(m_Shake, 0.15f);
            }
        }

        if (m_NovaAge >= 0.0f)
            m_NovaAge += inDeltaTime;
    }

    void FireBolts(int inLevel, float inDamageMultiplier)
    {
        int target = -1;
        float target_distance = 18.0f;

        for (size_t index = 0; index < m_Enemies.size(); index++)
        {
            const Enemy& enemy = m_Enemies[index];

            if (!enemy.mActive || enemy.mDying)
                continue;

            const float distance = glm::distance(enemy.mPosition, m_PlayerPosition);

            if (distance < target_distance)
            {
                target = int(index);
                target_distance = distance;
            }
        }

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
    }

    void UpdateProjectiles(float inDeltaTime)
    {
        for (Projectile& projectile : m_Projectiles)
        {
            if (!projectile.mActive)
                continue;

            projectile.mPosition += projectile.mVelocity * inDeltaTime;
            projectile.mLifetime -= inDeltaTime;

            ForEachEnemyNear(projectile.mPosition, 0.2f, [&](int inIndex, Enemy& inEnemy)
            {
                if (projectile.mPierce <= 0 || inIndex == projectile.mLastHit)
                    return;

                projectile.mLastHit = inIndex;
                projectile.mPierce--;

                DamageEnemy(inIndex, projectile.mDamage, glm::normalize(projectile.mVelocity) * 4.0f);
            });

            const bool outside = glm::any(glm::greaterThan(glm::abs(projectile.mPosition), Vec2(cArenaHalfSize)));

            if (projectile.mPierce <= 0 || projectile.mLifetime <= 0.0f || outside)
            {
                projectile.mActive = false;
                Park(projectile.mEntity);
            }
        }
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

        SetMaterial(target->mEntity, inValue > 1 ? m_LargeGemMaterial : m_SmallGemMaterial);
    }

    void UpdateGems(float inDeltaTime)
    {
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
                Park(gem.mEntity);
                AddExperience(gem.mValue);
            }
        }
    }

    void AddExperience(int inValue)
    {
        m_Experience += inValue;

        while (m_Experience >= GetExperienceToNextLevel())
        {
            m_Experience -= GetExperienceToNextLevel();
            m_Level++;
            m_PendingLevelUps++;
        }
    }

    void UpdateProgress()
    {
        if (m_RunTime >= cVictoryTime)
        {
            m_State = STATE_VICTORY;
            m_EndTime = m_Time;
            m_BestTime = glm::max(m_BestTime, m_RunTime);
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
    }

    void ChooseUpgrade(int inChoice)
    {
        if (inChoice < 0 || inChoice >= m_ChoiceCount)
            return;

        const int upgrade = m_Choices[inChoice];

        if (upgrade == UPGRADE_FEAST)
        {
            m_Health = glm::min(GetMaxHealth(), m_Health + GetMaxHealth() * 0.3f);
        }
        else
        {
            m_UpgradeLevels[upgrade]++;

            if (upgrade == UPGRADE_VITALITY)
                m_Health = glm::min(GetMaxHealth(), m_Health + 25.0f);

            if (upgrade == UPGRADE_NOVA && m_UpgradeLevels[upgrade] == 1)
                m_NovaTimer = 0.5f;
        }

        AddPopup(upgrade == UPGRADE_FEAST ? "FEAST" : std::format("{}  LV {}", cUpgrades[upgrade].mName, m_UpgradeLevels[upgrade]), Vec4(cUpgrades[upgrade].mColor, 1.0f));

        m_ChoiceCount = 0;
        m_State = STATE_PLAYING;
    }

    void UpdateVisuals(float inDeltaTime)
    {
        const bool moving = glm::length(m_PlayerVelocity) > 0.5f;

        m_PlayerYaw = glm::mix(m_PlayerYaw, m_PlayerYaw + glm::mod(GetYaw(m_FacingDirection) - m_PlayerYaw + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>(), 1.0f - glm::exp(-inDeltaTime * 14.0f));

        const float bob = moving ? glm::abs(glm::sin(m_Time * 11.0f)) * 0.12f : 0.0f;
        SetTransform(m_PlayerEntity, ToWorld(m_PlayerPosition, bob), Vec3(1.0f), Quat(Vec3(0.0f, m_PlayerYaw, 0.0f)));

        const bool player_flash = m_InvulnerableTimer > 0.0f && glm::fract(m_Time * 12.0f) < 0.5f;
        SetMaterial(m_PlayerHeadEntity, player_flash ? m_FlashMaterial : m_PlayerHeadMaterial);

        for (const Enemy& enemy : m_Enemies)
        {
            if (!enemy.mActive)
                continue;

            const EnemyArchetype& archetype = cEnemyArchetypes[enemy.mType];

            Vec3 scale = archetype.mScale * archetype.mRadius * 2.0f / glm::max(archetype.mScale.x, archetype.mScale.z);
            float height = archetype.mHeight;

            switch (enemy.mType)
            {
                case ENEMY_GHOUL:
                    height += glm::abs(glm::sin(m_Time * 7.0f + enemy.mPhase)) * 0.12f;
                    break;
                case ENEMY_BAT:
                    height += glm::sin(m_Time * 5.0f + enemy.mPhase) * 0.2f;
                    scale *= Vec3(1.6f + 0.6f * glm::sin(m_Time * 28.0f + enemy.mPhase), 0.55f, 0.9f);
                    break;
                case ENEMY_BRUTE:
                    scale *= Vec3(1.0f, 1.0f + 0.05f * glm::sin(m_Time * 4.0f + enemy.mPhase), 1.0f);
                    break;
            }

            if (!archetype.mIsSphere)
                height = glm::max(height, scale.y * 0.5f);

            if (enemy.mDying)
            {
                const float t = glm::clamp(enemy.mDeathTimer / cDeathDuration, 0.0f, 1.0f);
                scale *= Vec3(1.0f + t * 0.6f, 1.0f - t * 0.9f, 1.0f + t * 0.6f);
                height *= 1.0f - t * 0.5f;
            }

            SetTransform(enemy.mEntity, ToWorld(enemy.mPosition, height), scale, Quat(Vec3(0.0f, enemy.mYaw, 0.0f)));
        }

        for (const Projectile& projectile : m_Projectiles)
        {
            if (projectile.mActive)
                SetTransform(projectile.mEntity, ToWorld(projectile.mPosition, 0.9f), Vec3(0.32f, 0.32f, 0.7f), Quat(Vec3(0.0f, GetYaw(projectile.mVelocity), 0.0f)));
        }

        for (const Gem& gem : m_Gems)
        {
            if (gem.mActive)
            {
                const float size = gem.mValue > 1 ? 0.42f : 0.28f;
                const float height = 0.35f + 0.08f * glm::sin(m_Time * 4.0f + gem.mPhase);
                SetTransform(gem.mEntity, ToWorld(gem.mPosition, height), Vec3(size, size * 1.4f, size), Quat(Vec3(0.0f, m_Time * 2.0f + gem.mPhase, glm::quarter_pi<float>())));
            }
        }

        const int blade_level = m_UpgradeLevels[UPGRADE_BLADES];
        const int blade_count = blade_level > 0 ? glm::min(1 + blade_level, cMaxBlades) : 0;
        const float blade_radius = 2.0f + 0.2f * blade_level;

        for (int blade = 0; blade < cMaxBlades; blade++)
        {
            if (blade >= blade_count)
            {
                Park(m_Blades[blade]);
                continue;
            }

            const float angle = m_BladeAngle + glm::two_pi<float>() * float(blade) / float(blade_count);
            const Vec2 position = m_PlayerPosition + Vec2(glm::cos(angle), glm::sin(angle)) * blade_radius;
            SetTransform(m_Blades[blade], ToWorld(position, 0.8f), Vec3(0.75f, 0.12f, 0.3f), Quat(Vec3(0.0f, -angle, 0.0f)));
        }

        const float nova_duration = 0.35f;

        if (m_NovaAge >= 0.0f && m_NovaAge < nova_duration)
        {
            const float t = m_NovaAge / nova_duration;
            const float radius = m_NovaRadius * ( 0.3f + 0.7f * glm::sqrt(t) );
            SetTransform(m_NovaEntity, ToWorld(m_PlayerPosition, 0.2f), Vec3(radius * 2.0f, 0.15f, radius * 2.0f));

            if (Material* material = FindComponent<Material>(m_NovaMaterial))
                material->albedo.a = 0.45f * ( 1.0f - t );
        }
        else
        {
            Park(m_NovaEntity);
            m_NovaAge = -1.0f;
        }
    }

    void UpdateCamera(float inDeltaTime)
    {
        Transform* transform = FindComponent<Transform>(m_CameraEntity);

        if (transform == nullptr)
            return;

        const Vec3 player = ToWorld(m_PlayerPosition, 0.8f);
        m_CameraTarget = glm::mix(m_CameraTarget, player, 1.0f - glm::exp(-inDeltaTime * 6.0f));

        m_Shake = glm::max(0.0f, m_Shake - inDeltaTime * 1.5f);
        m_HitFlash = glm::max(0.0f, m_HitFlash - inDeltaTime * 3.0f);

        const Vec3 shake = Vec3(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f)) * m_Shake * m_Shake;

        const float pitch = glm::radians(58.0f);
        const Vec3 position = m_CameraTarget + Vec3(0.0f, glm::sin(pitch), glm::cos(pitch)) * m_CameraDistance + shake;

        transform->position = position;
        transform->rotation = glm::quatLookAtRH(glm::normalize(m_CameraTarget - position), Camera::cUp);
        transform->Compose();
    }

    void AddDamageNumber(const Vec3& inPosition, int inValue)
    {
        if (m_DamageNumbers.size() >= cMaxDamageNumbers)
            m_DamageNumbers.erase(m_DamageNumbers.begin());

        m_DamageNumbers.push_back(DamageNumber { .mPosition = inPosition + Vec3(RandomFloat(-0.3f, 0.3f), 0.0f, RandomFloat(-0.3f, 0.3f)), .mTime = 0.0f, .mValue = inValue });
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

    void DrawPanel(Vec2 inPos, Vec2 inSize, float inScale, Vec4 inBorder = Vec4(1.0f, 0.6f, 0.3f, 0.35f))
    {
        g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 6.0f) * inScale, inSize, 14.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.35f), 14.0f * inScale);
        g_UIRenderer.AddRectFilled(inPos, inSize, 14.0f * inScale, Vec4(0.06f, 0.04f, 0.05f, 0.8f));
        g_UIRenderer.AddRect(inPos, inSize, 14.0f * inScale, 1.5f * inScale, inBorder);
    }

    void DrawBar(Vec2 inPos, Vec2 inSize, float inFill, Vec4 inColor, float inScale)
    {
        const float radius = inSize.y * 0.5f;
        g_UIRenderer.AddRectFilled(inPos, inSize, radius, Vec4(0.0f, 0.0f, 0.0f, 0.6f));

        if (inFill > 0.0f)
            g_UIRenderer.AddRectFilled(inPos, Vec2(glm::max(inSize.x * glm::clamp(inFill, 0.0f, 1.0f), inSize.y), inSize.y), radius, inColor);
    }

    static String FormatTime(float inSeconds)
    {
        const int seconds = int(inSeconds);
        return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
    }

    void DrawHUD(float inDeltaTime)
    {
        const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
        const float scale = glm::max(display.y / 1080.0f, 0.5f);
        const float margin = 24.0f * scale;

        if (m_HitFlash > 0.0f)
            g_UIRenderer.AddRect(Vec2(0.0f), display, 0.0f, 90.0f * scale, Vec4(0.8f, 0.05f, 0.05f, 0.35f * m_HitFlash));

        if (m_State == STATE_PLAYING)
            DrawDamageNumbers(inDeltaTime, scale);

        const float xp_fill = float(m_Experience) / float(GetExperienceToNextLevel());
        DrawBar(Vec2(margin, margin * 0.6f), Vec2(display.x - margin * 2.0f, 14.0f * scale), xp_fill, Vec4(0.3f, 0.75f, 1.0f, 1.0f), scale);
        g_UIRenderer.AddText(Vec2(display.x - margin, margin * 0.6f + 20.0f * scale), std::format("LV {}", m_Level), 22.0f * scale, Vec4(0.6f, 0.85f, 1.0f, 1.0f), UI_TEXT_ALIGN_RIGHT);

        const float remaining = glm::max(0.0f, cVictoryTime - m_RunTime);
        g_UIRenderer.AddText(Vec2(display.x * 0.5f, margin * 0.6f + 22.0f * scale), FormatTime(m_RunTime), 44.0f * scale, Vec4(1.0f), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(display.x * 0.5f, margin * 0.6f + 72.0f * scale), std::format("dawn in {}", FormatTime(remaining)), 16.0f * scale, Vec4(1.0f, 0.75f, 0.5f, 0.8f), UI_TEXT_ALIGN_CENTER);

        const Vec2 stats_pos = Vec2(margin, margin * 0.6f + 30.0f * scale);
        DrawPanel(stats_pos, Vec2(230.0f, 96.0f) * scale, scale);
        g_UIRenderer.AddText(stats_pos + Vec2(18.0f, 12.0f) * scale, "DUSK SWARM", 18.0f * scale, Vec4(1.0f, 0.6f, 0.3f, 1.0f));
        g_UIRenderer.AddText(stats_pos + Vec2(18.0f, 40.0f) * scale, std::format("HP  {} / {}", int(glm::ceil(m_Health)), int(GetMaxHealth())), 17.0f * scale, Vec4(1.0f, 0.55f, 0.5f, 1.0f));
        g_UIRenderer.AddText(stats_pos + Vec2(18.0f, 64.0f) * scale, std::format("KILLS  {}", m_Kills), 17.0f * scale, Vec4(0.9f, 0.9f, 0.95f, 1.0f));

        Vec2 player_screen;
        if (ProjectToScreen(ToWorld(m_PlayerPosition, -0.2f), player_screen))
        {
            const Vec2 size = Vec2(64.0f, 8.0f) * scale;
            DrawBar(player_screen - Vec2(size.x * 0.5f, -6.0f * scale), size, m_Health / GetMaxHealth(), Vec4(0.95f, 0.25f, 0.25f, 1.0f), scale);
        }

        DrawLoadout(display, scale, margin);

        if (m_RunTime < 10.0f && m_State == STATE_PLAYING)
        {
            const float alpha = glm::clamp(( 10.0f - m_RunTime ) / 1.5f, 0.0f, 1.0f);
            g_UIRenderer.AddText(Vec2(display.x * 0.5f, display.y - margin - 60.0f * scale), "WASD / ARROWS to move   -   weapons fire on their own", 20.0f * scale, Vec4(1.0f, 0.9f, 0.75f, alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddText(Vec2(display.x * 0.5f, display.y - margin - 30.0f * scale), "Collect gems to level up. Survive until dawn.", 18.0f * scale, Vec4(1.0f, 0.75f, 0.5f, alpha * 0.85f), UI_TEXT_ALIGN_CENTER);
        }

        DrawPopups(display, inDeltaTime, scale);

        if (m_State == STATE_LEVEL_UP)
            DrawLevelUp(display, scale);
        else if (m_State == STATE_GAME_OVER || m_State == STATE_VICTORY)
            DrawEndScreen(display, scale);
    }

    void DrawDamageNumbers(float inDeltaTime, float inScale)
    {
        for (DamageNumber& number : m_DamageNumbers)
        {
            number.mTime += inDeltaTime;

            Vec2 screen;
            if (!ProjectToScreen(number.mPosition + Vec3(0.0f, number.mTime * 1.5f, 0.0f), screen))
                continue;

            const float alpha = glm::clamp(( 0.7f - number.mTime ) / 0.25f, 0.0f, 1.0f);
            const float size = ( 18.0f + glm::min(float(number.mValue), 20.0f) * 0.4f ) * inScale;
            const String text = std::format("{}", number.mValue);

            g_UIRenderer.AddText(screen + Vec2(1.5f, 1.5f) * inScale, text, size, Vec4(0.0f, 0.0f, 0.0f, 0.7f * alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddText(screen, text, size, Vec4(1.0f, 0.95f, 0.8f, alpha), UI_TEXT_ALIGN_CENTER);
        }

        std::erase_if(m_DamageNumbers, [](const DamageNumber& inNumber) { return inNumber.mTime >= 0.7f; });
    }

    void DrawLoadout(const Vec2& inDisplay, float inScale, float inMargin)
    {
        int count = 0;

        for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
            count += m_UpgradeLevels[upgrade] > 0;

        const float line_height = 24.0f * inScale;
        const Vec2 panel_size = Vec2(250.0f * inScale, count * line_height + 24.0f * inScale);
        const Vec2 panel_pos = Vec2(inMargin, inDisplay.y - inMargin - panel_size.y);

        DrawPanel(panel_pos, panel_size, inScale);

        int line = 0;

        for (int upgrade = 0; upgrade < UPGRADE_COUNT; upgrade++)
        {
            const int level = m_UpgradeLevels[upgrade];

            if (level == 0)
                continue;

            const Vec2 pos = panel_pos + Vec2(16.0f * inScale, 12.0f * inScale + line * line_height);
            const Vec3 color = cUpgrades[upgrade].mColor;

            g_UIRenderer.AddCircleFilled(pos + Vec2(6.0f, 10.0f) * inScale, 5.0f * inScale, Vec4(color, 1.0f));
            g_UIRenderer.AddText(pos + Vec2(20.0f * inScale, 0.0f), cUpgrades[upgrade].mName, 16.0f * inScale, Vec4(0.95f, 0.92f, 0.9f, 1.0f));

            for (int pip = 0; pip < cMaxUpgradeLevel; pip++)
            {
                const Vec2 pip_pos = pos + Vec2(160.0f + pip * 13.0f, 4.0f) * inScale;
                g_UIRenderer.AddRectFilled(pip_pos, Vec2(9.0f, 12.0f) * inScale, 2.0f * inScale, pip < level ? Vec4(color, 1.0f) : Vec4(1.0f, 1.0f, 1.0f, 0.12f));
            }

            line++;
        }
    }

    void DrawPopups(const Vec2& inDisplay, float inDeltaTime, float inScale)
    {
        float popup_y = inDisplay.y * 0.24f;

        for (Popup& popup : m_Popups)
        {
            popup.mTime += inDeltaTime;

            const float fade_in = glm::clamp(popup.mTime / 0.15f, 0.0f, 1.0f);
            const float fade_out = glm::clamp(( 2.2f - popup.mTime ) / 0.4f, 0.0f, 1.0f);
            const float alpha = fade_in * fade_out;
            const float size = glm::mix(26.0f, 38.0f, fade_in) * inScale;

            g_UIRenderer.AddText(Vec2(inDisplay.x * 0.5f + 2.0f * inScale, popup_y + 3.0f * inScale), popup.mText, size, Vec4(0.0f, 0.0f, 0.0f, 0.5f * alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddText(Vec2(inDisplay.x * 0.5f, popup_y), popup.mText, size, Vec4(Vec3(popup.mColor), alpha), UI_TEXT_ALIGN_CENTER);

            popup_y += 46.0f * inScale;
        }

        std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= 2.2f; });
    }

    void DrawLevelUp(const Vec2& inDisplay, float inScale)
    {
        const float alpha = glm::clamp(( m_Time - m_LevelUpTime ) / 0.2f, 0.0f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.02f, 0.0f, 0.02f, 0.5f * alpha));
        g_UIRenderer.AddText(Vec2(inDisplay.x * 0.5f, inDisplay.y * 0.5f - 230.0f * inScale), "LEVEL UP", 60.0f * inScale, Vec4(1.0f, 0.75f, 0.35f, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(inDisplay.x * 0.5f, inDisplay.y * 0.5f - 160.0f * inScale), "Choose an upgrade", 20.0f * inScale, Vec4(1.0f, 0.9f, 0.8f, 0.8f * alpha), UI_TEXT_ALIGN_CENTER);

        const Vec2 card_size = Vec2(280.0f, 240.0f) * inScale;
        const float gap = 28.0f * inScale;
        const float total_width = m_ChoiceCount * card_size.x + ( m_ChoiceCount - 1 ) * gap;

        for (int choice = 0; choice < m_ChoiceCount; choice++)
        {
            const int upgrade = m_Choices[choice];
            const UpgradeInfo& info = cUpgrades[upgrade];
            const bool selected = choice == m_SelectedChoice;
            const float lift = selected ? 10.0f * inScale : 0.0f;

            const Vec2 pos = Vec2(inDisplay.x * 0.5f - total_width * 0.5f + choice * ( card_size.x + gap ), inDisplay.y * 0.5f - 110.0f * inScale - lift);

            DrawPanel(pos, card_size, inScale, Vec4(info.mColor, selected ? 1.0f : 0.35f));

            if (selected)
                g_UIRenderer.AddRect(pos - Vec2(4.0f * inScale), card_size + Vec2(8.0f * inScale), 18.0f * inScale, 2.0f * inScale, Vec4(info.mColor, 0.5f));

            const float center = pos.x + card_size.x * 0.5f;

            g_UIRenderer.AddText(Vec2(center, pos.y + 18.0f * inScale), std::format("[{}]", choice + 1), 18.0f * inScale, Vec4(1.0f, 1.0f, 1.0f, 0.5f * alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddCircleFilled(Vec2(center, pos.y + 78.0f * inScale), 18.0f * inScale, Vec4(info.mColor, alpha), 6.0f * inScale);
            g_UIRenderer.AddText(Vec2(center, pos.y + 110.0f * inScale), info.mName, 24.0f * inScale, Vec4(info.mColor, alpha), UI_TEXT_ALIGN_CENTER);

            const String level_text = upgrade == UPGRADE_FEAST ? String("") :
                m_UpgradeLevels[upgrade] == 0 ? String(info.mIsWeapon ? "NEW WEAPON" : "NEW") : std::format("LV {}  >  {}", m_UpgradeLevels[upgrade], m_UpgradeLevels[upgrade] + 1);

            g_UIRenderer.AddText(Vec2(center, pos.y + 146.0f * inScale), level_text, 16.0f * inScale, Vec4(1.0f, 0.85f, 0.45f, alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddText(Vec2(center, pos.y + 182.0f * inScale), info.mDescription, 14.0f * inScale, Vec4(0.9f, 0.88f, 0.86f, 0.9f * alpha), UI_TEXT_ALIGN_CENTER);
        }

        g_UIRenderer.AddText(Vec2(inDisplay.x * 0.5f, inDisplay.y * 0.5f + 170.0f * inScale), "1 / 2 / 3 to pick   -   A D to select, ENTER to confirm", 18.0f * inScale, Vec4(1.0f, 0.9f, 0.75f, 0.75f * alpha), UI_TEXT_ALIGN_CENTER);
    }

    void DrawEndScreen(const Vec2& inDisplay, float inScale)
    {
        const bool victory = m_State == STATE_VICTORY;
        const float alpha = glm::clamp(( m_Time - m_EndTime ) / 0.6f, 0.0f, 1.0f);
        const Vec4 accent = victory ? Vec4(1.0f, 0.8f, 0.4f, 1.0f) : Vec4(1.0f, 0.35f, 0.3f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.02f, 0.0f, 0.0f, 0.55f * alpha));

        const Vec2 size = Vec2(560.0f, 280.0f) * inScale;
        const Vec2 pos = inDisplay * 0.5f - size * 0.5f;

        g_UIRenderer.AddRectFilled(pos, size, 20.0f * inScale, Vec4(0.06f, 0.04f, 0.05f, 0.92f * alpha));
        g_UIRenderer.AddRect(pos, size, 20.0f * inScale, 2.0f * inScale, Vec4(Vec3(accent), 0.8f * alpha));

        const float center = inDisplay.x * 0.5f;
        g_UIRenderer.AddText(Vec2(center, pos.y + 28.0f * inScale), victory ? "DAWN BREAKS" : "YOU FELL", 64.0f * inScale, Vec4(Vec3(accent), alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(center, pos.y + 118.0f * inScale), std::format("Survived {}   Level {}   Kills {}", FormatTime(m_RunTime), m_Level, m_Kills), 22.0f * inScale, Vec4(1.0f, 1.0f, 1.0f, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(center, pos.y + 158.0f * inScale), std::format("Best {}", FormatTime(m_BestTime)), 18.0f * inScale, Vec4(1.0f, 0.85f, 0.6f, 0.8f * alpha), UI_TEXT_ALIGN_CENTER);

        const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
        g_UIRenderer.AddText(Vec2(center, pos.y + 212.0f * inScale), "Press ENTER to play again", 20.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, alpha * blink), UI_TEXT_ALIGN_CENTER);
    }

private:
    bool m_Started = false;
    EState m_State = STATE_PLAYING;
    float m_Time = 0.0f;
    float m_RunTime = 0.0f;
    float m_EndTime = 0.0f;
    float m_BestTime = 0.0f;
    std::mt19937 m_Random;

    Entity m_RootEntity = Entity::Null;
    Entity m_CameraEntity = Entity::Null;
    Entity m_PlayerEntity = Entity::Null;
    Entity m_PlayerHeadEntity = Entity::Null;
    Entity m_NovaEntity = Entity::Null;
    StaticArray<Entity, cMaxBlades> m_Blades;

    StaticArray<Entity, ENEMY_TYPE_COUNT> m_EnemyMaterials;
    Entity m_FlashMaterial = Entity::Null;
    Entity m_GroundMaterial = Entity::Null;
    Entity m_DirtMaterial = Entity::Null;
    Entity m_StoneMaterial = Entity::Null;
    Entity m_WallMaterial = Entity::Null;
    Entity m_WoodMaterial = Entity::Null;
    Entity m_LanternMaterial = Entity::Null;
    Entity m_PlayerBodyMaterial = Entity::Null;
    Entity m_PlayerHeadMaterial = Entity::Null;
    Entity m_VisorMaterial = Entity::Null;
    Entity m_BoltMaterial = Entity::Null;
    Entity m_BladeMaterial = Entity::Null;
    Entity m_NovaMaterial = Entity::Null;
    Entity m_SmallGemMaterial = Entity::Null;
    Entity m_LargeGemMaterial = Entity::Null;

    Vec2 m_PlayerPosition = Vec2(0.0f);
    Vec2 m_PlayerVelocity = Vec2(0.0f);
    Vec2 m_FacingDirection = Vec2(0.0f, -1.0f);
    float m_PlayerYaw = 0.0f;
    float m_Health = cPlayerBaseHealth;
    float m_InvulnerableTimer = 0.0f;

    int m_Level = 1;
    int m_Experience = 0;
    int m_PendingLevelUps = 0;
    int m_Kills = 0;
    StaticArray<int, UPGRADE_COUNT> m_UpgradeLevels = {};

    StaticArray<int, cChoiceCount> m_Choices = {};
    int m_ChoiceCount = 0;
    int m_SelectedChoice = 0;
    float m_LevelUpTime = 0.0f;

    Array<Enemy> m_Enemies;
    StaticArray<Array<int>, ENEMY_TYPE_COUNT> m_FreeEnemies;
    Array<Projectile> m_Projectiles;
    Array<Gem> m_Gems;
    Array<Obstacle> m_Obstacles;
    Array<Vec2> m_Pushes;

    Array<int> m_GridStart;
    Array<int> m_GridFill;
    Array<int> m_GridCells;
    Array<int> m_GridEntries;

    float m_SpawnAccumulator = 0.0f;
    float m_NextSwarmTime = cSwarmInterval;
    float m_NextBruteTime = cBruteInterval;
    float m_BoltTimer = 0.0f;
    float m_NovaTimer = 0.0f;
    float m_NovaAge = -1.0f;
    float m_NovaRadius = 0.0f;
    float m_BladeAngle = 0.0f;

    Array<DamageNumber> m_DamageNumbers;
    Array<Popup> m_Popups;

    Vec3 m_CameraTarget = Vec3(0.0f);
    float m_CameraDistance = 22.0f;
    float m_Shake = 0.0f;
    float m_HitFlash = 0.0f;

public:
    DuskSwarmScript()
    {
        m_Blades.fill(Entity::Null);
        m_EnemyMaterials.fill(Entity::Null);
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
