#define RAEKOR_SCRIPT
#include "../Engine/Raekor.h"

namespace RK {

static constexpr int cWellWidth = 5;
static constexpr int cWellDepth = 5;
static constexpr int cWellHeight = 12;
static constexpr int cWellCellCount = cWellWidth * cWellDepth * cWellHeight;
static constexpr int cLayerCellCount = cWellWidth * cWellDepth;
static constexpr int cPieceCubeCount = 4;
static constexpr int cPieceTypeCount = 8;

static constexpr float cBlockScale = 0.94f;
static constexpr float cLockDelay = 0.5f;
static constexpr int   cMaxLockResets = 12;
static constexpr float cSoftDropInterval = 0.04f;
static constexpr float cClearFlashDuration = 0.4f;
static constexpr float cDebrisLifetime = 3.5f;
static constexpr int   cLayersPerLevel = 5;

static constexpr const char* cRuntimeRootName = "Tetris3D Runtime";


struct PieceShape
{
    const char* mName;
    StaticArray<IVec3, cPieceCubeCount> mCells;
    Vec3 mColor;
};


static const StaticArray<PieceShape, cPieceTypeCount> cPieceShapes =
{
    PieceShape { "I",      { IVec3(-1, 0, 0), IVec3(0, 0, 0), IVec3(1, 0, 0), IVec3(2, 0, 0) }, Vec3(0.10f, 0.75f, 0.95f) },
    PieceShape { "O",      { IVec3(0, 0, 0),  IVec3(1, 0, 0), IVec3(0, 0, 1), IVec3(1, 0, 1) }, Vec3(0.98f, 0.82f, 0.12f) },
    PieceShape { "T",      { IVec3(-1, 0, 0), IVec3(0, 0, 0), IVec3(1, 0, 0), IVec3(0, 0, 1) }, Vec3(0.68f, 0.28f, 0.92f) },
    PieceShape { "L",      { IVec3(-1, 0, 0), IVec3(0, 0, 0), IVec3(1, 0, 0), IVec3(1, 0, 1) }, Vec3(0.98f, 0.50f, 0.10f) },
    PieceShape { "S",      { IVec3(-1, 0, 0), IVec3(0, 0, 0), IVec3(0, 0, 1), IVec3(1, 0, 1) }, Vec3(0.22f, 0.85f, 0.35f) },
    PieceShape { "Tripod", { IVec3(0, 0, 0),  IVec3(1, 0, 0), IVec3(0, 0, 1), IVec3(0, 1, 0) }, Vec3(0.95f, 0.22f, 0.30f) },
    PieceShape { "Screw",  { IVec3(0, 0, 0),  IVec3(1, 0, 0), IVec3(0, 0, 1), IVec3(0, 1, 1) }, Vec3(0.20f, 0.40f, 0.98f) },
    PieceShape { "Twist",  { IVec3(0, 0, 0),  IVec3(1, 0, 0), IVec3(0, 0, 1), IVec3(1, 1, 0) }, Vec3(0.95f, 0.45f, 0.75f) },
};


class Tetris3DScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(Tetris3DScript);

    enum EState
    {
        STATE_FALLING,
        STATE_CLEARING,
        STATE_GAME_OVER
    };

    struct Popup
    {
        String mText;
        float mTime = 0.0f;
        Vec4 mColor = Vec4(1.0f);
    };

    struct Debris
    {
        Entity mEntity = Entity::Null;
        float mTime = 0.0f;
    };

    void OnStart() override
    {
        DestroyRuntimeEntities();

        m_Random.seed(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

        m_RootEntity = m_Scene->CreateSpatialEntity(cRuntimeRootName);

        CreateMaterials();
        CreateWell();
        CreateCamera();

        for (Entity& entity : m_GhostEntities)
            entity = CreateBlock("Ghost", m_GhostMaterial, Vec3(0.0f));

        m_NextPivotEntity = CreateEntity("Next Piece");

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

        inDeltaTime = glm::min(inDeltaTime, 0.1f);

        m_Time += inDeltaTime;

        switch (m_State)
        {
            case STATE_FALLING:  UpdateFalling(inDeltaTime);  break;
            case STATE_CLEARING: UpdateClearing(inDeltaTime); break;
            case STATE_GAME_OVER: break;
        }

        UpdateDebris(inDeltaTime);
        UpdateVisuals(inDeltaTime);
        UpdateCamera(inDeltaTime);
        DrawHUD(inDeltaTime);
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (!m_Started)
            return;

        if (inEvent.type == SDL_EVENT_MOUSE_WHEEL)
            m_CameraDistance = glm::clamp(m_CameraDistance - inEvent.wheel.y * 1.0f, 9.0f, 30.0f);

        if (inEvent.type != SDL_EVENT_KEY_DOWN)
            return;

        const SDL_Keycode key = inEvent.key.key;

        if (key == SDLK_LEFT && !inEvent.key.repeat)
            m_CameraTurns--;
        else if (key == SDLK_RIGHT && !inEvent.key.repeat)
            m_CameraTurns++;

        if (m_State == STATE_GAME_OVER)
        {
            if (key == SDLK_RETURN && !inEvent.key.repeat)
                NewGame();

            return;
        }

        if (m_State != STATE_FALLING)
            return;

        const IVec3 right = GetCameraRight();
        const IVec3 forward = GetCameraForward();

        switch (key)
        {
            case SDLK_W: TryMove(forward);  break;
            case SDLK_S: TryMove(-forward); break;
            case SDLK_A: TryMove(-right);   break;
            case SDLK_D: TryMove(right);    break;
        }

        if (inEvent.key.repeat)
            return;

        switch (key)
        {
            case SDLK_Q: TryRotate(IVec3(0, 1, 0));   break;
            case SDLK_E: TryRotate(IVec3(0, -1, 0));  break;
            case SDLK_R: TryRotate(right);            break;
            case SDLK_F: TryRotate(-right);           break;
            case SDLK_Z: TryRotate(forward);          break;
            case SDLK_X: TryRotate(-forward);         break;
            case SDLK_SPACE: HardDrop();              break;
        }
    }

private:
    Entity CreateEntity(StringView inName)
    {
        const Entity entity = m_Scene->CreateSpatialEntity(inName);
        m_Scene->ParentTo(entity, m_RootEntity);
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

    Entity CreateBlock(StringView inName, Entity inMaterial, Vec3 inPosition, Vec3 inScale = Vec3(cBlockScale), Entity inParent = Entity::Null)
    {
        const Entity entity = CreateEntity(inName);

        if (inParent != Entity::Null)
            m_Scene->ParentTo(entity, inParent);

        Transform& transform = m_Scene->Get<Transform>(entity);
        transform.position = inPosition;
        transform.scale = inScale;
        transform.Compose();

        Mesh& mesh = m_Scene->Add<Mesh>(entity);
        Mesh::CreateCube(mesh, 1.0f);
        mesh.material = inMaterial;

        m_App->GetRenderInterface()->UploadMeshBuffers(entity, mesh);

        return entity;
    }

    void CreateMaterials()
    {
        for (int index = 0; index < cPieceTypeCount; index++)
            m_PieceMaterials[index] = CreateMaterial(cPieceShapes[index].mColor, cPieceShapes[index].mColor * 1500.0f, 0.3f);

        m_GhostMaterial = CreateMaterial(Vec3(1.0f), Vec3(1.0f) * 2500.0f, 0.1f, 0.0f, 0.18f);
        m_FlashMaterial = CreateMaterial(Vec3(1.0f), Vec3(1.0f), 0.2f);
        m_FloorMaterial = CreateMaterial(Vec3(0.09f, 0.10f, 0.13f), Vec3(0.0f), 0.6f);
        m_TileMaterials[0] = CreateMaterial(Vec3(0.20f, 0.22f, 0.28f), Vec3(0.0f), 0.45f);
        m_TileMaterials[1] = CreateMaterial(Vec3(0.26f, 0.28f, 0.35f), Vec3(0.0f), 0.45f);
        m_FrameMaterial = CreateMaterial(Vec3(0.30f, 0.85f, 1.0f), Vec3(0.30f, 0.85f, 1.0f) * 12000.0f, 0.2f, 0.8f);
        m_GroundMaterial = CreateMaterial(Vec3(0.55f, 0.56f, 0.6f), Vec3(0.0f), 0.85f);
    }

    void CreateWell()
    {
        const Vec3 ground_size = Vec3(48.0f, 1.0f, 48.0f);
        const Entity ground = CreateBlock("Ground", m_GroundMaterial, Vec3(0.0f, -0.75f, 0.0f), ground_size);

        Physics& physics = *GetPhysics();
        RigidBody& ground_body = m_Scene->Add<RigidBody>(ground);
        ground_body.motionType = JPH::EMotionType::Static;
        ground_body.CreateCubeCollider(physics, BBox3D(-ground_size * 0.5f, ground_size * 0.5f));
        ground_body.CreateBody(physics, m_Scene->Get<Transform>(ground));
        ground_body.ActivateBody(physics, m_Scene->Get<Transform>(ground));

        CreateBlock("Well Floor", m_FloorMaterial, Vec3(0.0f, -0.15f, 0.0f), Vec3(cWellWidth + 0.6f, 0.3f, cWellDepth + 0.6f));

        for (int z = 0; z < cWellDepth; z++)
        {
            for (int x = 0; x < cWellWidth; x++)
            {
                const Vec3 position = Vec3(GetCellPosition(IVec3(x, 0, z)).x, 0.02f, GetCellPosition(IVec3(x, 0, z)).z);
                CreateBlock("Well Tile", m_TileMaterials[( x + z ) % 2], position, Vec3(0.96f, 0.04f, 0.96f));
            }
        }

        const float half_width = cWellWidth * 0.5f;
        const float half_depth = cWellDepth * 0.5f;
        const float bar = 0.07f;

        for (int corner = 0; corner < 4; corner++)
        {
            const float x = ( corner & 1 ) ? half_width : -half_width;
            const float z = ( corner & 2 ) ? half_depth : -half_depth;
            CreateBlock("Well Pillar", m_FrameMaterial, Vec3(x, cWellHeight * 0.5f, z), Vec3(bar, float(cWellHeight), bar));
        }

        for (const float y : { 0.0f, float(cWellHeight) })
        {
            CreateBlock("Well Rim", m_FrameMaterial, Vec3(0.0f, y, -half_depth), Vec3(cWellWidth + bar, bar, bar));
            CreateBlock("Well Rim", m_FrameMaterial, Vec3(0.0f, y, half_depth), Vec3(cWellWidth + bar, bar, bar));
            CreateBlock("Well Rim", m_FrameMaterial, Vec3(-half_width, y, 0.0f), Vec3(bar, bar, cWellDepth + bar));
            CreateBlock("Well Rim", m_FrameMaterial, Vec3(half_width, y, 0.0f), Vec3(bar, bar, cWellDepth + bar));
        }
    }

    void CreateCamera()
    {
        m_CameraEntity = CreateEntity("Tetris Camera");

        Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
        camera.SetFov(55.0f);

        m_CameraYaw = float(m_CameraTurns) * 90.0f;
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
        m_NextPivotEntity = Entity::Null;
        m_Board.fill(Entity::Null);
        m_PieceEntities.fill(Entity::Null);
        m_NextEntities.fill(Entity::Null);
        m_GhostEntities.fill(Entity::Null);
        m_Debris.clear();
    }

    void NewGame()
    {
        for (Entity& entity : m_Board)
        {
            if (entity != Entity::Null)
                m_Scene->Destroy(entity);

            entity = Entity::Null;
        }

        for (Entity& entity : m_PieceEntities)
        {
            if (entity != Entity::Null)
                m_Scene->Destroy(entity);

            entity = Entity::Null;
        }

        m_Score = 0;
        m_Level = 1;
        m_LayersCleared = 0;
        m_PiecesPlaced = 0;
        m_Bag.clear();
        m_Popups.clear();
        m_ClearingLayers.clear();
        m_State = STATE_FALLING;

        m_NextType = DrawFromBag();
        SpawnPiece();
    }

    int DrawFromBag()
    {
        if (m_Bag.empty())
        {
            for (int type = 0; type < cPieceTypeCount; type++)
                m_Bag.push_back(type);

            std::shuffle(m_Bag.begin(), m_Bag.end(), m_Random);
        }

        const int type = m_Bag.back();
        m_Bag.pop_back();
        return type;
    }

    void SpawnPiece()
    {
        m_PieceType = m_NextType;
        m_NextType = DrawFromBag();

        m_PieceCells = cPieceShapes[m_PieceType].mCells;

        IVec3 min_cell = IVec3(INT_MAX);
        IVec3 max_cell = IVec3(INT_MIN);

        for (const IVec3& cell : m_PieceCells)
        {
            min_cell = glm::min(min_cell, cell);
            max_cell = glm::max(max_cell, cell);
        }

        const IVec3 size = max_cell - min_cell + 1;
        m_PiecePosition = IVec3(( cWellWidth - size.x ) / 2 - min_cell.x, cWellHeight - 1 - max_cell.y, ( cWellDepth - size.z ) / 2 - min_cell.z);

        m_FallTimer = 0.0f;
        m_LockTimer = 0.0f;
        m_LockResets = 0;

        for (int index = 0; index < cPieceCubeCount; index++)
        {
            if (m_PieceEntities[index] != Entity::Null)
                m_Scene->Destroy(m_PieceEntities[index]);

            m_PieceEntities[index] = CreateBlock(cPieceShapes[m_PieceType].mName, m_PieceMaterials[m_PieceType], GetCellPosition(m_PiecePosition + m_PieceCells[index]) + Vec3(0.0f, 1.5f, 0.0f));
        }

        m_PieceVisualPosition = Vec3(m_PiecePosition) + Vec3(0.0f, 1.5f, 0.0f);

        RebuildNextPreview();

        if (!Fits(m_PieceCells, m_PiecePosition))
        {
            m_State = STATE_GAME_OVER;
            m_GameOverTime = m_Time;
            m_BestScore = glm::max(m_BestScore, m_Score);
        }
    }

    void RebuildNextPreview()
    {
        for (Entity& entity : m_NextEntities)
        {
            if (entity != Entity::Null)
                m_Scene->Destroy(entity);
        }

        const PieceShape& shape = cPieceShapes[m_NextType];

        Vec3 center = Vec3(0.0f);
        for (const IVec3& cell : shape.mCells)
            center += Vec3(cell);

        center /= float(cPieceCubeCount);

        for (int index = 0; index < cPieceCubeCount; index++)
            m_NextEntities[index] = CreateBlock("Next", m_PieceMaterials[m_NextType], ( Vec3(shape.mCells[index]) - center ) * 0.6f, Vec3(cBlockScale * 0.6f), m_NextPivotEntity);
    }

    static int GetCellIndex(const IVec3& inCell)
    {
        return inCell.x + inCell.z * cWellWidth + inCell.y * cLayerCellCount;
    }

    static bool IsInsideWell(const IVec3& inCell)
    {
        return inCell.x >= 0 && inCell.x < cWellWidth && inCell.z >= 0 && inCell.z < cWellDepth && inCell.y >= 0 && inCell.y < cWellHeight;
    }

    static Vec3 GetCellPosition(const IVec3& inCell)
    {
        return GetCellPosition(Vec3(inCell));
    }

    static Vec3 GetCellPosition(const Vec3& inCell)
    {
        return Vec3(inCell.x - ( cWellWidth - 1 ) * 0.5f, inCell.y + 0.5f, inCell.z - ( cWellDepth - 1 ) * 0.5f);
    }

    bool Fits(const StaticArray<IVec3, cPieceCubeCount>& inCells, const IVec3& inPosition) const
    {
        for (const IVec3& offset : inCells)
        {
            const IVec3 cell = inPosition + offset;

            if (cell.x < 0 || cell.x >= cWellWidth || cell.z < 0 || cell.z >= cWellDepth || cell.y < 0)
                return false;

            if (cell.y < cWellHeight && m_Board[GetCellIndex(cell)] != Entity::Null)
                return false;
        }

        return true;
    }

    int GetDropDistance() const
    {
        int distance = 0;

        while (Fits(m_PieceCells, m_PiecePosition - IVec3(0, distance + 1, 0)))
            distance++;

        return distance;
    }

    bool TryMove(const IVec3& inDirection)
    {
        if (!Fits(m_PieceCells, m_PiecePosition + inDirection))
            return false;

        m_PiecePosition += inDirection;
        OnPieceManipulated();
        return true;
    }

    static IVec3 RotateCell(const IVec3& inCell, const IVec3& inAxis)
    {
        if (inAxis.x > 0) return IVec3(inCell.x, -inCell.z, inCell.y);
        if (inAxis.x < 0) return IVec3(inCell.x, inCell.z, -inCell.y);
        if (inAxis.y > 0) return IVec3(inCell.z, inCell.y, -inCell.x);
        if (inAxis.y < 0) return IVec3(-inCell.z, inCell.y, inCell.x);
        if (inAxis.z > 0) return IVec3(-inCell.y, inCell.x, inCell.z);
        if (inAxis.z < 0) return IVec3(inCell.y, -inCell.x, inCell.z);
        return inCell;
    }

    bool TryRotate(const IVec3& inAxis)
    {
        StaticArray<IVec3, cPieceCubeCount> rotated;

        for (int index = 0; index < cPieceCubeCount; index++)
            rotated[index] = RotateCell(m_PieceCells[index], inAxis);

        static constexpr std::array cKicks =
        {
            IVec3(0, 0, 0),
            IVec3(1, 0, 0), IVec3(-1, 0, 0), IVec3(0, 0, 1), IVec3(0, 0, -1),
            IVec3(0, 1, 0),
            IVec3(2, 0, 0), IVec3(-2, 0, 0), IVec3(0, 0, 2), IVec3(0, 0, -2),
            IVec3(0, -1, 0)
        };

        for (const IVec3& kick : cKicks)
        {
            if (Fits(rotated, m_PiecePosition + kick))
            {
                m_PieceCells = rotated;
                m_PiecePosition += kick;
                OnPieceManipulated();
                return true;
            }
        }

        return false;
    }

    void OnPieceManipulated()
    {
        if (m_LockTimer > 0.0f && m_LockResets < cMaxLockResets)
        {
            m_LockTimer = 0.0f;
            m_LockResets++;
        }
    }

    float GetFallInterval() const
    {
        return glm::max(0.06f, 1.0f * glm::pow(0.8f, float(m_Level - 1)));
    }

    void UpdateFalling(float inDeltaTime)
    {
        const bool soft_drop = m_Input->IsKeyDown(Key::LSHIFT);
        const float interval = soft_drop ? glm::min(cSoftDropInterval, GetFallInterval()) : GetFallInterval();

        if (Fits(m_PieceCells, m_PiecePosition - IVec3(0, 1, 0)))
        {
            m_LockTimer = 0.0f;
            m_FallTimer += inDeltaTime;

            while (m_FallTimer >= interval && Fits(m_PieceCells, m_PiecePosition - IVec3(0, 1, 0)))
            {
                m_FallTimer -= interval;
                m_PiecePosition.y--;

                if (soft_drop)
                    m_Score += 1;
            }
        }
        else
        {
            m_FallTimer = 0.0f;
            m_LockTimer += inDeltaTime;

            if (m_LockTimer >= cLockDelay || m_LockResets >= cMaxLockResets)
                LockPiece();
        }
    }

    void HardDrop()
    {
        const int distance = GetDropDistance();

        m_PiecePosition.y -= distance;
        m_Score += distance * 2;

        LockPiece();
    }

    void LockPiece()
    {
        bool above_well = false;

        for (int index = 0; index < cPieceCubeCount; index++)
        {
            const IVec3 cell = m_PiecePosition + m_PieceCells[index];

            if (cell.y >= cWellHeight)
            {
                above_well = true;
                m_Scene->Destroy(m_PieceEntities[index]);
            }
            else
            {
                m_Board[GetCellIndex(cell)] = m_PieceEntities[index];
                m_BlockTypes[GetCellIndex(cell)] = m_PieceType;
            }

            m_PieceEntities[index] = Entity::Null;
        }

        m_PiecesPlaced++;

        if (above_well)
        {
            m_State = STATE_GAME_OVER;
            m_GameOverTime = m_Time;
            m_BestScore = glm::max(m_BestScore, m_Score);
            return;
        }

        m_ClearingLayers.clear();

        for (int y = 0; y < cWellHeight; y++)
        {
            if (GetLayerFillCount(y) == cLayerCellCount)
                m_ClearingLayers.push_back(y);
        }

        if (m_ClearingLayers.empty())
        {
            SpawnPiece();
            return;
        }

        for (int y : m_ClearingLayers)
        {
            for (int cell = 0; cell < cLayerCellCount; cell++)
            {
                if (Mesh* mesh = FindComponent<Mesh>(m_Board[y * cLayerCellCount + cell]))
                    mesh->material = m_FlashMaterial;
            }
        }

        m_State = STATE_CLEARING;
        m_ClearTimer = 0.0f;
    }

    int GetLayerFillCount(int inLayer) const
    {
        int count = 0;

        for (int cell = 0; cell < cLayerCellCount; cell++)
            count += m_Board[inLayer * cLayerCellCount + cell] != Entity::Null;

        return count;
    }

    void UpdateClearing(float inDeltaTime)
    {
        m_ClearTimer += inDeltaTime;

        const float pulse = 0.5f + 0.5f * glm::cos(m_ClearTimer * 40.0f);

        if (Material* material = FindComponent<Material>(m_FlashMaterial))
            material->emissive = Vec3(1.0f, 0.95f, 0.85f) * glm::mix(20000.0f, 90000.0f, pulse);

        if (m_ClearTimer < cClearFlashDuration)
            return;

        for (int y : m_ClearingLayers)
        {
            for (int cell = 0; cell < cLayerCellCount; cell++)
            {
                Entity& entity = m_Board[y * cLayerCellCount + cell];
                SpawnDebris(entity, m_BlockTypes[y * cLayerCellCount + cell]);
                entity = Entity::Null;
            }
        }

        for (int index = int(m_ClearingLayers.size()) - 1; index >= 0; index--)
        {
            const int cleared_layer = m_ClearingLayers[index];

            for (int y = cleared_layer; y < cWellHeight - 1; y++)
            {
                for (int cell = 0; cell < cLayerCellCount; cell++)
                {
                    m_Board[y * cLayerCellCount + cell] = m_Board[( y + 1 ) * cLayerCellCount + cell];
                    m_BlockTypes[y * cLayerCellCount + cell] = m_BlockTypes[( y + 1 ) * cLayerCellCount + cell];
                }
            }

            for (int cell = 0; cell < cLayerCellCount; cell++)
                m_Board[( cWellHeight - 1 ) * cLayerCellCount + cell] = Entity::Null;
        }

        const int cleared_count = int(m_ClearingLayers.size());
        static constexpr std::array cLayerScores = { 0, 100, 300, 700, 1500 };
        static constexpr std::array<const char*, 5> cLayerNames = { "", "LAYER", "DOUBLE", "TRIPLE", "QUADRUPLE" };

        const int clamped_count = glm::min(cleared_count, 4);
        m_Score += cLayerScores[clamped_count] * m_Level;
        m_LayersCleared += cleared_count;

        AddPopup(std::format("{}  +{}", cLayerNames[clamped_count], cLayerScores[clamped_count] * m_Level), Vec4(1.0f, 0.85f, 0.35f, 1.0f));

        bool is_empty = true;
        for (Entity entity : m_Board)
            is_empty &= entity == Entity::Null;

        if (is_empty)
        {
            m_Score += 2000 * m_Level;
            AddPopup(std::format("PERFECT CLEAR  +{}", 2000 * m_Level), Vec4(0.45f, 1.0f, 0.65f, 1.0f));
        }

        const int new_level = 1 + m_LayersCleared / cLayersPerLevel;

        if (new_level > m_Level)
        {
            m_Level = new_level;
            AddPopup(std::format("LEVEL {}", m_Level), Vec4(0.45f, 0.85f, 1.0f, 1.0f));
        }

        m_ClearingLayers.clear();
        m_State = STATE_FALLING;

        SpawnPiece();
    }

    void SpawnDebris(Entity inEntity, int inType)
    {
        if (inEntity == Entity::Null)
            return;

        Physics& physics = *GetPhysics();

        if (Mesh* mesh = FindComponent<Mesh>(inEntity))
            mesh->material = m_PieceMaterials[inType];

        Transform& transform = m_Scene->Get<Transform>(inEntity);

        RigidBody& rigid_body = m_Scene->Add<RigidBody>(inEntity);
        rigid_body.motionType = JPH::EMotionType::Dynamic;
        rigid_body.CreateCubeCollider(physics, BBox3D(Vec3(-cBlockScale * 0.5f), Vec3(cBlockScale * 0.5f)));
        rigid_body.CreateBody(physics, transform);
        rigid_body.ActivateBody(physics, transform);

        if (rigid_body.bodyID.IsInvalid())
        {
            m_Scene->Destroy(inEntity);
            return;
        }

        std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

        Vec3 outward = Vec3(transform.position.x, 0.0f, transform.position.z);
        outward = glm::length(outward) > 0.01f ? glm::normalize(outward) : Vec3(distribution(m_Random), 0.0f, distribution(m_Random));

        const Vec3 velocity = outward * ( 5.0f + 3.0f * distribution(m_Random) ) + Vec3(0.0f, 6.0f + 2.0f * distribution(m_Random), 0.0f);
        const Vec3 spin = Vec3(distribution(m_Random), distribution(m_Random), distribution(m_Random)) * 8.0f;

        JPH::BodyInterface& bodies = physics.GetSystem()->GetBodyInterface();
        bodies.SetLinearVelocity(rigid_body.bodyID, JPH::Vec3(velocity.x, velocity.y, velocity.z));
        bodies.SetAngularVelocity(rigid_body.bodyID, JPH::Vec3(spin.x, spin.y, spin.z));

        m_Debris.push_back(Debris { .mEntity = inEntity, .mTime = 0.0f });
    }

    void UpdateDebris(float inDeltaTime)
    {
        for (Debris& debris : m_Debris)
        {
            debris.mTime += inDeltaTime;

            Transform* transform = FindComponent<Transform>(debris.mEntity);

            if (transform == nullptr)
                continue;

            const float shrink = glm::clamp(( cDebrisLifetime - debris.mTime ) / 1.0f, 0.05f, 1.0f);
            transform->scale = Vec3(cBlockScale * shrink);
            transform->Compose();

            if (debris.mTime >= cDebrisLifetime)
            {
                if (RigidBody* rigid_body = FindComponent<RigidBody>(debris.mEntity))
                    rigid_body->DestroyBody(*GetPhysics());

                m_Scene->Destroy(debris.mEntity);
            }
        }

        std::erase_if(m_Debris, [](const Debris& inDebris) { return inDebris.mTime >= cDebrisLifetime; });
    }

    void UpdateVisuals(float inDeltaTime)
    {
        const float smoothing = 1.0f - glm::exp(-inDeltaTime * 22.0f);

        for (int index = 0; index < cWellCellCount; index++)
        {
            Transform* transform = FindComponent<Transform>(m_Board[index]);

            if (transform == nullptr)
                continue;

            const IVec3 cell = IVec3(index % cWellWidth, index / cLayerCellCount, ( index / cWellWidth ) % cWellDepth);
            transform->position = glm::mix(transform->position, GetCellPosition(cell), smoothing);
            transform->scale = Vec3(cBlockScale);
            transform->rotation = Quat(Vec3(0.0f));
            transform->Compose();
        }

        const bool show_piece = m_State == STATE_FALLING;

        m_PieceVisualPosition = glm::mix(m_PieceVisualPosition, Vec3(m_PiecePosition), smoothing);

        const float lock_pulse = m_LockTimer > 0.0f ? 0.06f * glm::sin(m_Time * 30.0f) : 0.0f;

        for (int index = 0; index < cPieceCubeCount; index++)
        {
            Transform* transform = FindComponent<Transform>(m_PieceEntities[index]);

            if (transform == nullptr)
                continue;

            transform->position = GetCellPosition(m_PieceVisualPosition + Vec3(m_PieceCells[index]));
            transform->scale = Vec3(cBlockScale + lock_pulse);
            transform->Compose();
        }

        const int drop_distance = show_piece ? GetDropDistance() : 0;

        for (int index = 0; index < cPieceCubeCount; index++)
        {
            Transform* transform = FindComponent<Transform>(m_GhostEntities[index]);

            if (transform == nullptr)
                continue;

            transform->position = GetCellPosition(m_PiecePosition + m_PieceCells[index] - IVec3(0, drop_distance, 0));
            transform->scale = Vec3(cBlockScale * 0.98f);
            transform->Compose();
        }

        if (Transform* transform = FindComponent<Transform>(m_NextPivotEntity))
        {
            const Vec3 offset = glm::rotate(Quat(Vec3(0.0f, glm::radians(m_CameraYaw), 0.0f)), Vec3(cWellWidth * 0.5f + 2.6f, 0.0f, 0.0f));

            transform->position = Vec3(offset.x, cWellHeight - 2.0f, offset.z);
            transform->rotation = Quat(Vec3(glm::radians(20.0f), m_Time * 0.9f, 0.0f));
            transform->Compose();
        }

        if (Material* ghost = FindComponent<Material>(m_GhostMaterial))
        {
            const Vec3 color = cPieceShapes[m_PieceType].mColor;
            ghost->albedo = Vec4(color, 0.22f);
            ghost->emissive = color * 4000.0f;
        }
    }

    int GetCameraQuadrant() const
    {
        return ( ( m_CameraTurns % 4 ) + 4 ) % 4;
    }

    IVec3 GetCameraRight() const
    {
        static constexpr std::array cRights = { IVec3(1, 0, 0), IVec3(0, 0, -1), IVec3(-1, 0, 0), IVec3(0, 0, 1) };
        return cRights[GetCameraQuadrant()];
    }

    IVec3 GetCameraForward() const
    {
        static constexpr std::array cForwards = { IVec3(0, 0, -1), IVec3(-1, 0, 0), IVec3(0, 0, 1), IVec3(1, 0, 0) };
        return cForwards[GetCameraQuadrant()];
    }

    void UpdateCamera(float inDeltaTime)
    {
        Transform* transform = FindComponent<Transform>(m_CameraEntity);

        if (transform == nullptr)
            return;

        m_CameraYaw = glm::mix(m_CameraYaw, float(m_CameraTurns) * 90.0f, 1.0f - glm::exp(-inDeltaTime * 10.0f));

        const Vec3 target = Vec3(0.0f, cWellHeight * 0.5f, 0.0f);
        const float pitch = glm::radians(55.0f);
        const Vec3 offset = Vec3(0.0f, glm::sin(pitch), glm::cos(pitch)) * m_CameraDistance;
        const Vec3 position = target + glm::rotate(Quat(Vec3(0.0f, glm::radians(m_CameraYaw), 0.0f)), offset);

        transform->position = position;
        transform->rotation = glm::quatLookAtRH(glm::normalize(target - position), Camera::cUp);
        transform->Compose();
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

    void DrawPanel(Vec2 inPos, Vec2 inSize, float inScale)
    {
        g_UIRenderer.AddRectFilled(inPos + Vec2(0.0f, 6.0f) * inScale, inSize, 14.0f * inScale, Vec4(0.0f, 0.0f, 0.0f, 0.35f), 14.0f * inScale);
        g_UIRenderer.AddRectFilled(inPos, inSize, 14.0f * inScale, Vec4(0.04f, 0.05f, 0.08f, 0.78f));
        g_UIRenderer.AddRect(inPos, inSize, 14.0f * inScale, 1.5f * inScale, Vec4(0.35f, 0.85f, 1.0f, 0.35f));
    }

    void DrawStat(Vec2 inPos, const char* inLabel, const String& inValue, float inScale)
    {
        g_UIRenderer.AddText(inPos, inLabel, 15.0f * inScale, Vec4(0.55f, 0.75f, 0.9f, 1.0f));
        g_UIRenderer.AddText(inPos + Vec2(0.0f, 20.0f) * inScale, inValue, 38.0f * inScale, Vec4(1.0f));
    }

    void DrawHUD(float inDeltaTime)
    {
        const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
        const float scale = glm::max(display.y / 1080.0f, 0.5f);
        const float margin = 32.0f * scale;

        const Vec2 panel_pos = Vec2(margin);
        const Vec2 panel_size = Vec2(240.0f, 300.0f) * scale;

        DrawPanel(panel_pos, panel_size, scale);

        g_UIRenderer.AddText(panel_pos + Vec2(22.0f, 16.0f) * scale, "BLOCK DROP 3D", 20.0f * scale, Vec4(0.35f, 0.85f, 1.0f, 1.0f));

        DrawStat(panel_pos + Vec2(22.0f, 56.0f) * scale, "SCORE", std::format("{}", m_Score), scale);
        DrawStat(panel_pos + Vec2(22.0f, 136.0f) * scale, "LEVEL", std::format("{}", m_Level), scale);
        DrawStat(panel_pos + Vec2(130.0f, 136.0f) * scale, "LAYERS", std::format("{}", m_LayersCleared), scale);
        DrawStat(panel_pos + Vec2(22.0f, 216.0f) * scale, "BEST", std::format("{}", glm::max(m_BestScore, m_Score)), scale);

        DrawLayerMeter(display, scale, margin);
        DrawControls(display, scale, margin);

        if (m_State != STATE_GAME_OVER)
        {
            Vec2 next_screen;
            if (Transform* transform = FindComponent<Transform>(m_NextPivotEntity))
            {
                if (ProjectToScreen(transform->position + Vec3(0.0f, 1.4f, 0.0f), next_screen))
                    g_UIRenderer.AddText(next_screen, "NEXT", 18.0f * scale, Vec4(0.75f, 0.9f, 1.0f, 0.9f), UI_TEXT_ALIGN_CENTER);
            }
        }

        float popup_y = display.y * 0.32f;

        for (Popup& popup : m_Popups)
        {
            popup.mTime += inDeltaTime;

            const float fade_in = glm::clamp(popup.mTime / 0.15f, 0.0f, 1.0f);
            const float fade_out = glm::clamp(( 1.8f - popup.mTime ) / 0.4f, 0.0f, 1.0f);
            const float alpha = fade_in * fade_out;
            const float rise = popup.mTime * 30.0f * scale;
            const float size = glm::mix(30.0f, 44.0f, fade_in) * scale;

            g_UIRenderer.AddText(Vec2(display.x * 0.5f + 2.0f * scale, popup_y - rise + 3.0f * scale), popup.mText, size, Vec4(0.0f, 0.0f, 0.0f, 0.5f * alpha), UI_TEXT_ALIGN_CENTER);
            g_UIRenderer.AddText(Vec2(display.x * 0.5f, popup_y - rise), popup.mText, size, Vec4(Vec3(popup.mColor), popup.mColor.a * alpha), UI_TEXT_ALIGN_CENTER);

            popup_y += 52.0f * scale;
        }

        std::erase_if(m_Popups, [](const Popup& inPopup) { return inPopup.mTime >= 1.8f; });

        if (m_State == STATE_GAME_OVER)
            DrawGameOver(display, scale);
    }

    void DrawLayerMeter(const Vec2& inDisplay, float inScale, float inMargin)
    {
        const float cell_height = 22.0f * inScale;
        const float gap = 4.0f * inScale;
        const float width = 28.0f * inScale;
        const float height = cWellHeight * ( cell_height + gap ) - gap;

        const Vec2 origin = Vec2(inDisplay.x - inMargin - width, inDisplay.y * 0.5f + height * 0.5f);

        DrawPanel(origin - Vec2(12.0f * inScale, height + 12.0f * inScale), Vec2(width, height) + 24.0f * inScale, inScale);

        for (int y = 0; y < cWellHeight; y++)
        {
            const float fill = float(GetLayerFillCount(y)) / float(cLayerCellCount);
            const Vec2 pos = origin - Vec2(0.0f, ( y + 1 ) * ( cell_height + gap ) - gap);

            Vec4 color = Vec4(1.0f, 1.0f, 1.0f, 0.06f);

            if (fill > 0.0f)
            {
                const Vec3 rgb = glm::mix(Vec3(0.25f, 0.85f, 0.45f), Vec3(1.0f, 0.3f, 0.25f), glm::clamp(float(y) / float(cWellHeight - 3), 0.0f, 1.0f));
                color = Vec4(rgb, glm::mix(0.35f, 1.0f, fill));
            }

            g_UIRenderer.AddRectFilled(pos, Vec2(width, cell_height), 4.0f * inScale, color);
        }

        g_UIRenderer.AddText(Vec2(origin.x + width * 0.5f, origin.y + 20.0f * inScale), "HEIGHT", 13.0f * inScale, Vec4(0.55f, 0.75f, 0.9f, 1.0f), UI_TEXT_ALIGN_CENTER);
    }

    void DrawControls(const Vec2& inDisplay, float inScale, float inMargin)
    {
        static constexpr std::array<std::pair<const char*, const char*>, 8> cControls =
        {
            std::pair { "W A S D",     "Move" },
            std::pair { "Q / E",       "Spin" },
            std::pair { "R / F",       "Tip forward / back" },
            std::pair { "Z / X",       "Roll left / right" },
            std::pair { "SHIFT",       "Soft drop" },
            std::pair { "SPACE",       "Hard drop" },
            std::pair { "LEFT / RIGHT", "Orbit camera" },
            std::pair { "ESC",         "Pause" },
        };

        const float line_height = 22.0f * inScale;
        const Vec2 panel_size = Vec2(300.0f * inScale, cControls.size() * line_height + 28.0f * inScale);
        const Vec2 panel_pos = Vec2(inMargin, inDisplay.y - inMargin - panel_size.y);

        DrawPanel(panel_pos, panel_size, inScale);

        for (size_t index = 0; index < cControls.size(); index++)
        {
            const Vec2 pos = panel_pos + Vec2(18.0f * inScale, 14.0f * inScale + index * line_height);
            g_UIRenderer.AddText(pos, cControls[index].first, 15.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, 1.0f));
            g_UIRenderer.AddText(pos + Vec2(120.0f * inScale, 0.0f), cControls[index].second, 15.0f * inScale, Vec4(0.85f, 0.9f, 1.0f, 0.9f));
        }
    }

    void DrawGameOver(const Vec2& inDisplay, float inScale)
    {
        const float alpha = glm::clamp(( m_Time - m_GameOverTime ) / 0.5f, 0.0f, 1.0f);

        g_UIRenderer.AddRectFilled(Vec2(0.0f), inDisplay, 0.0f, Vec4(0.0f, 0.0f, 0.02f, 0.55f * alpha));

        const Vec2 size = Vec2(520.0f, 250.0f) * inScale;
        const Vec2 pos = inDisplay * 0.5f - size * 0.5f;

        g_UIRenderer.AddRectFilled(pos, size, 20.0f * inScale, Vec4(0.04f, 0.05f, 0.08f, 0.92f * alpha));
        g_UIRenderer.AddRect(pos, size, 20.0f * inScale, 2.0f * inScale, Vec4(1.0f, 0.35f, 0.3f, 0.8f * alpha));

        const float center = inDisplay.x * 0.5f;
        g_UIRenderer.AddText(Vec2(center, pos.y + 28.0f * inScale), "GAME OVER", 64.0f * inScale, Vec4(1.0f, 0.4f, 0.35f, alpha), UI_TEXT_ALIGN_CENTER);
        g_UIRenderer.AddText(Vec2(center, pos.y + 118.0f * inScale), std::format("Score {}   Layers {}   Level {}", m_Score, m_LayersCleared, m_Level), 22.0f * inScale, Vec4(1.0f, 1.0f, 1.0f, alpha), UI_TEXT_ALIGN_CENTER);

        const float blink = 0.6f + 0.4f * glm::sin(m_Time * 4.0f);
        g_UIRenderer.AddText(Vec2(center, pos.y + 182.0f * inScale), "Press ENTER to play again", 20.0f * inScale, Vec4(1.0f, 0.85f, 0.4f, alpha * blink), UI_TEXT_ALIGN_CENTER);
    }

private:
    bool m_Started = false;
    EState m_State = STATE_FALLING;
    float m_Time = 0.0f;
    std::mt19937 m_Random;

    Entity m_RootEntity = Entity::Null;
    Entity m_CameraEntity = Entity::Null;
    Entity m_NextPivotEntity = Entity::Null;

    StaticArray<Entity, cPieceTypeCount> m_PieceMaterials;
    StaticArray<Entity, 2> m_TileMaterials;
    Entity m_GhostMaterial = Entity::Null;
    Entity m_FlashMaterial = Entity::Null;
    Entity m_FloorMaterial = Entity::Null;
    Entity m_FrameMaterial = Entity::Null;
    Entity m_GroundMaterial = Entity::Null;

    StaticArray<Entity, cWellCellCount> m_Board;
    StaticArray<int, cWellCellCount> m_BlockTypes = {};

    int m_PieceType = 0;
    int m_NextType = 0;
    IVec3 m_PiecePosition = IVec3(0);
    Vec3 m_PieceVisualPosition = Vec3(0.0f);
    StaticArray<IVec3, cPieceCubeCount> m_PieceCells;
    StaticArray<Entity, cPieceCubeCount> m_PieceEntities;
    StaticArray<Entity, cPieceCubeCount> m_GhostEntities;
    StaticArray<Entity, cPieceCubeCount> m_NextEntities;
    Array<int> m_Bag;

    float m_FallTimer = 0.0f;
    float m_LockTimer = 0.0f;
    int m_LockResets = 0;

    float m_ClearTimer = 0.0f;
    Array<int> m_ClearingLayers;
    Array<Debris> m_Debris;
    Array<Popup> m_Popups;

    int m_Score = 0;
    int m_BestScore = 0;
    int m_Level = 1;
    int m_LayersCleared = 0;
    int m_PiecesPlaced = 0;
    float m_GameOverTime = 0.0f;

    int m_CameraTurns = 0;
    float m_CameraYaw = 0.0f;
    float m_CameraDistance = 18.0f;

public:
    Tetris3DScript()
    {
        m_Board.fill(Entity::Null);
        m_PieceEntities.fill(Entity::Null);
        m_GhostEntities.fill(Entity::Null);
        m_NextEntities.fill(Entity::Null);
        m_PieceMaterials.fill(Entity::Null);
        m_TileMaterials.fill(Entity::Null);
    }
};


RTTI_DEFINE_TYPE(Tetris3DScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(Tetris3DScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(Tetris3DScript, SERIALIZE_ALL, "Best Score", m_BestScore);
    RTTI_DEFINE_SCRIPT_MEMBER(Tetris3DScript, SERIALIZE_ALL, "Camera Distance", m_CameraDistance);
}

RK_REGISTER_SCRIPT(Tetris3DScript)

} // RK
