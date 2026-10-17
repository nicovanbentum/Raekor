#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

uint32_t CoastRallyScript::LoadTexture(const char* inFile, bool inSRGB)
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


void CoastRallyScript::LoadTextures()
{
    if (m_PaletteTexture == 0)
        m_PaletteTexture = LoadTexture(cPaletteFile, true);

    if (m_PaletteEmissiveTexture == 0)
        m_PaletteEmissiveTexture = LoadTexture(cPaletteEmissiveFile, false);

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

    m_WaterMaterial = CreateMaterial(Vec3(0.03f, 0.2f, 0.28f), Vec3(0.0f), 0.08f);
    m_BrakeMaterial = CreateMaterial(Vec3(1.0f, 0.1f, 0.06f), Vec3(1.0f, 0.1f, 0.06f) * cBrakeLightIntensity, 0.3f);
}


void CoastRallyScript::CreateLevel()
{
    m_LevelTriangles.clear();

    for (const char* asset : cLevelAssets)
    {
        SetTransform(CreateModel(asset).mRoot, Vec3(0.0f));

        if (const auto templates = m_MeshTemplates.find(asset); templates != m_MeshTemplates.end())
            AddCollision(templates->second, Mat4x4(1.0f));
    }

    SetTransform(CreateModel("Water", m_WaterMaterial).mRoot, Vec3(0.0f));

    for (const Placement& placement : m_Placements)
    {
        SetTransform(CreateModel(placement.mAsset).mRoot, placement.mPosition, placement.mRotation, placement.mScale);

        if (std::find(cDecorationAssets.begin(), cDecorationAssets.end(), placement.mAsset) != cDecorationAssets.end())
            continue;

        const Mat4x4 transform = glm::scale(glm::translate(Mat4x4(1.0f), placement.mPosition) * glm::toMat4(placement.mRotation), placement.mScale);

        if (const auto collision = m_CollisionTemplates.find(placement.mAsset); collision != m_CollisionTemplates.end())
            AddCollision(collision->second, transform);
        else if (const auto templates = m_MeshTemplates.find(placement.mAsset); templates != m_MeshTemplates.end())
            AddCollision(templates->second, transform);
    }

    CreateLevelBody();
}


void CoastRallyScript::AddCollision(const Array<PartTemplate>& inParts, const Mat4x4& inTransform)
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

            m_LevelTriangles.push_back(JPH::Triangle(vertices[0], vertices[1], vertices[2]));
        }
    }
}


void CoastRallyScript::CreateLevelBody()
{
    if (m_LevelTriangles.empty())
        return;

    JPH::MeshShapeSettings shape_settings(m_LevelTriangles);
    const JPH::ShapeSettings::ShapeResult shape = shape_settings.Create();

    m_LevelTriangles.clear();
    m_LevelTriangles.shrink_to_fit();

    if (shape.HasError())
    {
        Log(std::format("Coast Rally: failed to build the level collision ({})", shape.GetError().c_str()));
        return;
    }

    JPH::BodyCreationSettings settings(shape.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static, EPhysicsObjectLayers::NON_MOVING);
    settings.mFriction = 1.0f;

    m_LevelBody = GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}


void CoastRallyScript::DestroyLevelBody()
{
    if (m_LevelBody.IsInvalid())
        return;

    JPH::BodyInterface& bodies = GetBodyInterface();
    bodies.RemoveBody(m_LevelBody);
    bodies.DestroyBody(m_LevelBody);

    m_LevelBody = JPH::BodyID();
}


void CoastRallyScript::CreateCamera()
{
    m_CameraEntity = CreateEntity("Rally Camera");

    Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
    camera.SetFov(cChaseFov);

    m_App->SetCameraEntity(m_CameraEntity);
}

} // RK
