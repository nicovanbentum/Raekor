#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

String CoastRallyScript::ConvertTexture(const String& inFile)
{
    const String cached = TextureAsset::GetCachedPath(inFile);

    if (!fs::exists(cached) || fs::last_write_time(cached) < fs::last_write_time(inFile))
        TextureAsset::Convert(inFile);

    return cached;
}


uint32_t CoastRallyScript::LoadTexture(const char* inFile, bool inSRGB, uint8_t inSwizzle)
{
    if (!fs::exists(inFile))
        return 0;

    const String cached = ConvertTexture(inFile);

    Assets* assets = m_App->GetAssets();
    TextureAsset::Ptr asset = assets ? assets->GetAsset<TextureAsset>(cached) : nullptr;

    return asset ? m_App->GetRenderInterface()->UploadTextureFromAsset(asset, inSRGB, inSwizzle) : 0;
}


uint32_t CoastRallyScript::LoadCarTexture(const String& inCachedFile, bool inSRGB, uint8_t inSwizzle, uint32_t inDefault)
{
    if (inCachedFile.empty())
        return inDefault;

    const String key = std::format("{}:{}", inCachedFile, inSwizzle);

    if (const auto texture = m_CarTextures.find(key); texture != m_CarTextures.end())
        return texture->second;

    Assets* assets = m_App->GetAssets();
    TextureAsset::Ptr asset = assets ? assets->GetAsset<TextureAsset>(inCachedFile) : nullptr;

    const uint32_t texture = asset ? m_App->GetRenderInterface()->UploadTextureFromAsset(asset, inSRGB, inSwizzle) : inDefault;
    m_CarTextures[key] = texture;

    return texture;
}


void CoastRallyScript::LoadTextures()
{
    if (m_PaletteTexture == 0)
        m_PaletteTexture = LoadTexture(cPaletteFile, true);

    if (m_PaletteEmissiveTexture == 0)
        m_PaletteEmissiveTexture = LoadTexture(cPaletteEmissiveFile, false);

    if (m_PaletteRoughnessTexture == 0)
        m_PaletteRoughnessTexture = LoadTexture(cPaletteSurfaceFile, false, TEXTURE_SWIZZLE_GGGG);

    if (m_PaletteMetallicTexture == 0)
        m_PaletteMetallicTexture = LoadTexture(cPaletteSurfaceFile, false, TEXTURE_SWIZZLE_BBBB);

    if (m_SkyVisibilityTexture == 0)
        m_SkyVisibilityTexture = LoadTexture(cSkyVisibilityFile, false);

    if (m_GroundAlbedoTexture == 0)
        m_GroundAlbedoTexture = LoadTexture(cGroundAlbedoFile, true);

    if (m_TerrainAlbedoTexture == 0)
        m_TerrainAlbedoTexture = LoadTexture(cTerrainAlbedoFile, true);

    if (m_TerrainNormalTexture == 0)
        m_TerrainNormalTexture = LoadTexture(cTerrainNormalFile, false, cTerrainNormalSwizzle);

    if (m_TerrainDetailTexture == 0)
        m_TerrainDetailTexture = LoadTexture(cTerrainDetailFile, true);

    if (m_TerrainDetailNormalTexture == 0)
        m_TerrainDetailNormalTexture = LoadTexture(cTerrainDetailNormalFile, false, cTerrainNormalSwizzle);

    if (m_BeamGradientTexture == 0)
        m_BeamGradientTexture = LoadTexture(cBeamGradientFile, false);

    m_HudFont = g_UIRenderer.AddFont(cHudFontFile);
    m_LabelFont = g_UIRenderer.AddFont(cLabelFontFile);
}


void CoastRallyScript::ReleaseModels()
{
    m_MeshTemplates.clear();
    m_CollisionTemplates.clear();
}


void CoastRallyScript::LoadModels()
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

        auto& templates = part == "Collision" ? m_CollisionTemplates : m_MeshTemplates;

        PartTemplate& part_template = templates[asset].emplace_back();
        part_template.mName = part;
        part_template.mJoint = scene.Get<Transform>(entity).position;
        part_template.mMesh = mesh;
    }

    Array<std::pair<int, Waypoint>> waypoints;

    for (const auto& [entity, name, transform] : scene.Each<Name, Transform>())
    {
        if (scene.Has<Mesh>(entity))
            continue;

        const Placement placement = Placement
        {
            .mPosition = transform.position,
            .mRotation = transform.rotation,
            .mScale = transform.scale
        };

        const size_t first = name.name.find('_');
        const size_t second = name.name.find('_', first + 1);

        if (name.name == "Spawn")
        {
            m_Spawn = placement;
        }
        else if (name.name.starts_with("Road_"))
        {
            const int index = std::atoi(name.name.substr(name.name.rfind('_') + 1).c_str());
            waypoints.emplace_back(index,Waypoint { .mPosition = transform.position, .mForward = transform.rotation * Vec3(0.0f, 0.0f, 1.0f) });
        }
        else if (second != String::npos && ( name.name.starts_with("Place_") || name.name.starts_with("Prop_") ))
        {
            Placement& target = name.name.starts_with("Prop_") ? m_PropPlacements.emplace_back(placement) : m_Placements.emplace_back(placement);
            target.mAsset = name.name.substr(first + 1, second - first - 1);
        }
    }

    std::sort(waypoints.begin(), waypoints.end(), [](const auto& inA, const auto& inB) { return inA.first < inB.first; });

    m_Waypoints.clear();

    for (const auto& [index, waypoint] : waypoints)
        m_Waypoints.push_back(waypoint);

    Log(std::format("Coast Rally: {} placements, {} props, {} waypoints", m_Placements.size(), m_PropPlacements.size(), m_Waypoints.size()));
}


CoastRallyScript::Model CoastRallyScript::CreateModel(const String& inName, Entity inMaterial)
{
    Model model;
    model.mRoot = m_Nodes.Create();

    const Entity material = inMaterial != Entity::Null ? inMaterial : m_PaletteMaterial;
    const auto templates = m_MeshTemplates.find(inName);

    if (templates == m_MeshTemplates.end())
    {
        Mesh cube;
        cube.SetAsset(MeshAsset::Find(*m_App->GetAssets(), MeshAsset::cBuiltinCube), 0);

        const Node fallback = m_Nodes.CreateMesh(cube, material, model.mRoot);
        SetTransform(fallback, Vec3(0.0f, 0.5f, 0.0f));

        model.mParts.push_back(ModelPart { .mName = "Main", .mNode = fallback });
        return model;
    }

    for (const PartTemplate& part : templates->second)
    {
        const Node node = m_Nodes.CreateMesh(part.mMesh, material, model.mRoot);
        SetTransform(node, part.mJoint);
        model.mParts.push_back(ModelPart { .mName = part.mName, .mNode = node });
    }

    return model;
}


void CoastRallyScript::CreateMaterials()
{
    m_PaletteMaterial = CreateMaterial(Vec3(1.0f), Vec3(0.0f), 0.85f);

    if (Material* palette = FindComponent<Material>(m_PaletteMaterial))
    {
        palette->gpuAlbedoMap = m_PaletteTexture;
        palette->gpuEmissiveMap = m_PaletteEmissiveTexture;
        palette->emissive = m_PaletteEmissiveTexture != 0 ? Vec3(20000.0f) : Vec3(0.0f);
    }

    m_TerrainMaterial = CreateMaterial(Vec3(1.0f), Vec3(0.0f), 0.9f);

    if (Material* terrain = FindComponent<Material>(m_TerrainMaterial))
    {
        terrain->gpuAlbedoMap = m_TerrainAlbedoTexture != 0 ? m_TerrainAlbedoTexture : m_PaletteTexture;
        terrain->gpuNormalMap = m_TerrainNormalTexture;
        terrain->gpuDetailMap = m_TerrainDetailTexture;
        terrain->gpuDetailNormalMap = m_TerrainDetailNormalTexture;
        terrain->detailTiling = cTerrainDetailTiling;
        m_App->GetRenderInterface()->UploadMaterialSampler(*terrain);
    }

    m_WaterMaterial = CreateMaterial(Vec3(0.03f, 0.2f, 0.28f), Vec3(0.0f), 0.08f);

    if (Material* water = FindComponent<Material>(m_WaterMaterial))
    {
        water->vertexShaderFile = cWaterVertexShaderFile;
        water->pixelShaderFile = cWaterPixelShaderFile;
        m_App->GetRenderInterface()->CompileMaterialShaders(m_WaterMaterial, *water);
    }
    m_BrakeMaterial = CreateMaterial(Vec3(1.0f, 0.1f, 0.06f), Vec3(1.0f, 0.1f, 0.06f) * cBrakeLightIntensity, 0.3f);

    m_BeamMaterial = CreateMaterial(Vec3(0.0f), Vec3(0.0f), 1.0f, 1.0f);

    if (Material* beam = FindComponent<Material>(m_BeamMaterial))
    {
        beam->albedo.a = 0.0f;
        beam->blendMode = MATERIAL_BLEND_MODE_BLENDED;
        beam->gpuEmissiveMap = m_BeamGradientTexture;
    }
}


void CoastRallyScript::CreateLevel()
{
    m_LevelTriangles.clear();
    m_ObstacleTriangles.clear();

    for (const char* asset : cLevelAssets)
    {
        SetTransform(CreateModel(asset, StringView(asset) == cTerrainAsset ? m_TerrainMaterial : Entity::Null).mRoot, Vec3(0.0f));

        if (const auto templates = m_MeshTemplates.find(asset); templates != m_MeshTemplates.end())
            AddCollision(m_LevelTriangles, templates->second, Mat4x4(1.0f));
    }

    SetTransform(CreateModel("Water", m_WaterMaterial).mRoot, Vec3(0.0f));

    for (const Placement& placement : m_Placements)
    {
        SetTransform(CreateModel(placement.mAsset).mRoot, placement.mPosition, placement.mRotation, placement.mScale);

        if (std::find(cDecorationAssets.begin(), cDecorationAssets.end(), placement.mAsset) != cDecorationAssets.end())
            continue;

        const Mat4x4 transform = glm::scale(glm::translate(Mat4x4(1.0f), placement.mPosition) * glm::toMat4(placement.mRotation), placement.mScale);
        const bool ground = std::find(cGroundPlacements.begin(), cGroundPlacements.end(), placement.mAsset) != cGroundPlacements.end();
        JPH::TriangleList& triangles = ground ? m_LevelTriangles : m_ObstacleTriangles;

        if (const auto collision = m_CollisionTemplates.find(placement.mAsset); collision != m_CollisionTemplates.end())
            AddCollision(triangles, collision->second, transform);
        else if (const auto templates = m_MeshTemplates.find(placement.mAsset); templates != m_MeshTemplates.end())
            AddCollision(triangles, templates->second, transform);
    }

    CreateNightLights();
    CreateLevelBodies();
}


void CoastRallyScript::AddCollision(JPH::TriangleList& outTriangles, const Array<PartTemplate>& inParts, const Mat4x4& inTransform)
{
    for (const PartTemplate& part : inParts)
    {
        const MeshGeometry* geometry = part.mMesh.GetGeometry();

        if (geometry == nullptr)
            continue;

        const Mat4x4 transform = glm::translate(inTransform, part.mJoint);

        for (size_t index = 0; index + 2 < geometry->indices.size(); index += 3)
        {
            StaticArray<JPH::Float3, 3> vertices;

            for (size_t corner = 0; corner < 3; corner++)
            {
                const Vec3 position = Vec3(transform * Vec4(geometry->positions[geometry->indices[index + corner]], 1.0f));
                vertices[corner] = JPH::Float3(position.x, position.y, position.z);
            }

            outTriangles.push_back(JPH::Triangle(vertices[0], vertices[1], vertices[2]));
        }
    }
}


JPH::BodyID CoastRallyScript::CreateStaticBody(JPH::TriangleList& ioTriangles, int inSubGroup)
{
    if (ioTriangles.empty())
        return JPH::BodyID();

    JPH::MeshShapeSettings shape_settings(ioTriangles);
    const JPH::ShapeSettings::ShapeResult shape = shape_settings.Create();

    ioTriangles.clear();
    ioTriangles.shrink_to_fit();

    if (shape.HasError())
    {
        Log(std::format("Coast Rally: failed to build the level collision ({})", shape.GetError().c_str()));
        return JPH::BodyID();
    }

    JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static, EPhysicsObjectLayers::NON_MOVING);
    settings.mFriction = 1.0f;

    if (inSubGroup >= 0)
        settings.mCollisionGroup = GetCollisionGroup(inSubGroup);

    return GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}


void CoastRallyScript::CreateLevelBodies()
{
    m_LevelBody = CreateStaticBody(m_LevelTriangles, -1);
    m_ObstacleBody = CreateStaticBody(m_ObstacleTriangles, COLLISION_SUBGROUP_OBSTACLE);
}


void CoastRallyScript::DestroyLevelBodies()
{
    JPH::BodyInterface& bodies = GetBodyInterface();

    for (JPH::BodyID* body : { &m_LevelBody, &m_ObstacleBody })
    {
        if (body->IsInvalid())
            continue;

        bodies.RemoveBody(*body);
        bodies.DestroyBody(*body);
        *body = JPH::BodyID();
    }
}


JPH::CollisionGroup CoastRallyScript::GetCollisionGroup(int inSubGroup) const
{
    return JPH::CollisionGroup(m_CollisionFilter, 0, JPH::CollisionGroup::SubGroupID(inSubGroup));
}


void CoastRallyScript::ApplyGodMode()
{
    if (m_CollisionFilter == nullptr)
        return;

    if (*m_GodMode)
        m_CollisionFilter->DisableCollision(COLLISION_SUBGROUP_CAR, COLLISION_SUBGROUP_OBSTACLE);
    else
        m_CollisionFilter->EnableCollision(COLLISION_SUBGROUP_CAR, COLLISION_SUBGROUP_OBSTACLE);
}


void CoastRallyScript::ToggleGodMode()
{
    *m_GodMode = *m_GodMode ? 0 : 1;
    ApplyGodMode();
    PlaySfx(cSelectSound, 0.5f);
}


void CoastRallyScript::CreateCamera()
{
    m_CameraEntity = CreateEntity("Rally Camera");

    Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
    camera.SetFov(cChaseFov);

    m_App->SetCameraEntity(m_CameraEntity);
}

} // RK
