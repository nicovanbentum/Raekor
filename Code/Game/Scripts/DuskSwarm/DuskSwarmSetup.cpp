#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

uint32_t DuskSwarmScript::LoadTexture(const char* inFile, bool inSRGB, uint8_t inSwizzle)
{
    if (!fs::exists(inFile))
        return 0;

    const String cached = TextureAsset::GetCachedPath(inFile);

    if (!fs::exists(cached) || fs::last_write_time(cached) < fs::last_write_time(inFile))
        TextureAsset::Convert(inFile);

    Assets* assets = m_App->GetAssets();
    TextureAsset::Ptr asset = assets ? assets->GetAsset<TextureAsset>(cached) : nullptr;

    return asset ? m_App->GetRenderInterface()->UploadTextureFromAsset(asset, inSRGB, inSwizzle) : 0;
}


void DuskSwarmScript::LoadTextures()
{
    if (m_PaletteTexture == 0)
        m_PaletteTexture = LoadTexture(cPaletteFile, true);

    if (m_PaletteEmissiveTexture == 0)
        m_PaletteEmissiveTexture = LoadTexture(cPaletteEmissiveFile, false);

    for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
    {
        GroundTextures& textures = m_GroundTextures[layer];
        const String prefix = std::format("Assets/DuskSwarm/Ground/{}", cGroundLayers[layer].mTextures);

        if (textures.mAlbedo == 0)
            textures.mAlbedo = LoadTexture(( prefix + "Albedo.png" ).c_str(), true);

        if (textures.mNormal == 0)
            textures.mNormal = LoadTexture(( prefix + "Normal.png" ).c_str(), false, cNormalMapSwizzleAG);

        if (textures.mRoughness == 0)
            textures.mRoughness = LoadTexture(( prefix + "Roughness.png" ).c_str(), false, TEXTURE_SWIZZLE_GGGG);
    }

    const auto LoadUITexture = [this](uint32_t& outTexture, const char* inFile)
    {
        if (outTexture == 0)
            outTexture = LoadTexture(inFile, false);
    };

    LoadUITexture(m_PanelTexture, "Assets/DuskSwarm/UI/Panel.png");
    LoadUITexture(m_ParchmentTexture, "Assets/DuskSwarm/UI/Parchment.png");
    LoadUITexture(m_SlotTexture, "Assets/DuskSwarm/UI/Slot.png");
    LoadUITexture(m_ButtonTexture, "Assets/DuskSwarm/UI/Button.png");
    LoadUITexture(m_OrbFrameTexture, "Assets/DuskSwarm/UI/OrbFrame.png");
    LoadUITexture(m_OrbFillTexture, "Assets/DuskSwarm/UI/OrbFill.png");
    LoadUITexture(m_HitsplatTexture, "Assets/DuskSwarm/UI/Hitsplat.png");
    LoadUITexture(m_HitsplatMaxTexture, "Assets/DuskSwarm/UI/HitsplatMax.png");
    LoadUITexture(m_HintArrowTexture, "Assets/DuskSwarm/UI/HintArrow.png");
    LoadUITexture(m_HeartTexture, "Assets/DuskSwarm/UI/Heart.png");
    LoadUITexture(m_GemTexture, "Assets/DuskSwarm/UI/Gem.png");
    LoadUITexture(m_DashIconTexture, "Assets/DuskSwarm/UI/Icons/Dash.png");

    for (int upgrade = 0; upgrade <= UPGRADE_COUNT; upgrade++)
        LoadUITexture(m_UpgradeIconTextures[upgrade], std::format("Assets/DuskSwarm/UI/Icons/{}.png", cUpgradeIcons[upgrade]).c_str());

    m_HudFont = g_UIRenderer.AddFont(cHudFontFile);
    m_TitleFont = g_UIRenderer.AddFont(cTitleFontFile);
}


void DuskSwarmScript::ReleaseModels()
{
    m_MeshTemplates.clear();
}


void DuskSwarmScript::LoadModels()
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

        PartTemplate& part_template = m_MeshTemplates[asset].emplace_back();
        part_template.mName = part;
        part_template.mJoint = scene.Get<Transform>(entity).position;
        part_template.mMesh = mesh;
    }
}


Array<DuskSwarmScript::Model> DuskSwarmScript::CreateModels(const String& inName, int inCount, Entity inMaterial)
{
    Array<Model> models(inCount);
    const Entity material = inMaterial != Entity::Null ? inMaterial : m_PaletteMaterial;

    for (Model& model : models)
        model.mRoot = m_Nodes.Create();

    const auto templates = m_MeshTemplates.find(inName);

    if (templates == m_MeshTemplates.end())
    {
        Mesh cube;
        cube.SetAsset(MeshAsset::Find(*m_App->GetAssets(), MeshAsset::cBuiltinCube), 0);

        for (Model& model : models)
        {
            const Node fallback = m_Nodes.CreateMesh(cube, material, model.mRoot);
            SetTransform(fallback, Vec3(0.0f, 0.5f, 0.0f), Vec3(0.6f));

            model.mParts[RIG_BODY] = RigPart { .mNode = fallback, .mRest = Vec3(0.0f, 0.5f, 0.0f) };
            model.mMeshes.push_back(ModelMesh { .mNode = fallback, .mMaterial = material });
        }

        return models;
    }

    for (const PartTemplate& part : templates->second)
    {
        const int role = GetRigPart(part.mName);

        for (Model& model : models)
        {
            const Node node = m_Nodes.CreateMesh(part.mMesh, material, model.mRoot);
            SetTransform(node, part.mJoint, Vec3(1.0f));

            if (role >= 0 && model.mParts[role].mNode == Node::Null)
                model.mParts[role] = RigPart { .mNode = node, .mRest = part.mJoint };

            model.mMeshes.push_back(ModelMesh { .mNode = node, .mMaterial = material });
        }
    }

    return models;
}


DuskSwarmScript::Model DuskSwarmScript::CreateModel(const String& inName, Entity inMaterial)
{
    return CreateModels(inName, 1, inMaterial)[0];
}


void DuskSwarmScript::CreateMaterials()
{
    m_PaletteMaterial = CreateMaterial(Vec3(1.0f), Vec3(0.0f), 0.85f);

    if (Material* palette = FindComponent<Material>(m_PaletteMaterial))
    {
        palette->gpuAlbedoMap = m_PaletteTexture;
        palette->gpuEmissiveMap = m_PaletteEmissiveTexture;
        palette->emissive = m_PaletteEmissiveTexture != 0 ? Vec3(16000.0f) : Vec3(0.0f);
    }

    for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
    {
        m_GroundMaterials[layer] = CreateMaterial(cGroundLayers[layer].mTint, Vec3(0.0f), 1.0f);

        if (Material* ground = FindComponent<Material>(m_GroundMaterials[layer]))
        {
            ground->gpuAlbedoMap = m_GroundTextures[layer].mAlbedo;
            ground->gpuNormalMap = m_GroundTextures[layer].mNormal;
            ground->gpuRoughnessMap = m_GroundTextures[layer].mRoughness;

            if (layer == GROUND_GRASS)
            {
                ground->samplerFilter = MATERIAL_SAMPLER_FILTER_POINT;
                m_App->GetRenderInterface()->UploadMaterialSampler(*ground);
            }
        }
    }

    m_FlashMaterial = CreateMaterial(Vec3(1.0f), Vec3(1.0f, 0.9f, 0.85f) * 5000.0f, 0.3f);
    m_EliteFlashMaterial = CreateMaterial(Vec3(1.0f, 0.85f, 0.4f), Vec3(1.0f, 0.75f, 0.3f) * 5000.0f, 0.3f);
    m_BoltMaterial = CreateMaterial(Vec3(0.35f, 0.85f, 1.0f), Vec3(0.35f, 0.85f, 1.0f) * 12000.0f, 0.2f);
    m_GemMaterials[0] = CreateMaterial(Vec3(0.3f, 1.0f, 0.5f), Vec3(0.3f, 1.0f, 0.5f) * 6000.0f, 0.2f);
    m_GemMaterials[1] = CreateMaterial(Vec3(0.3f, 0.6f, 1.0f), Vec3(0.3f, 0.6f, 1.0f) * 8000.0f, 0.2f);
    m_GemMaterials[2] = CreateMaterial(Vec3(1.0f, 0.3f, 0.3f), Vec3(1.0f, 0.25f, 0.2f) * 10000.0f, 0.2f);
    m_DangerMaterial = CreateMaterial(Vec3(0.35f, 0.04f, 0.03f), Vec3(1.0f, 0.12f, 0.08f) * 900.0f, 0.6f);
    m_PoisonTelegraphMaterial = CreateMaterial(Vec3(0.15f, 0.3f, 0.05f), Vec3(0.45f, 1.0f, 0.2f) * 900.0f, 0.6f);

    const auto Emissive = [this](Vec3 inColor, float inStrength) { return CreateMaterial(inColor, inColor * inStrength, 0.3f); };

    m_FXMaterials[FX_FIRE]   = Emissive(Vec3(1.0f, 0.4f, 0.1f), 3500.0f);
    m_FXMaterials[FX_EMBER]  = Emissive(Vec3(1.0f, 0.62f, 0.25f), 7000.0f);
    m_FXMaterials[FX_SPARK]  = Emissive(Vec3(1.0f, 0.92f, 0.75f), 8000.0f);
    m_FXMaterials[FX_ARCANE] = Emissive(Vec3(0.3f, 0.8f, 1.0f), 5000.0f);
    m_FXMaterials[FX_PURPLE] = Emissive(Vec3(0.7f, 0.3f, 1.0f), 4500.0f);
    m_FXMaterials[FX_POISON] = Emissive(Vec3(0.5f, 1.0f, 0.25f), 3500.0f);
    m_FXMaterials[FX_GOLD]   = Emissive(Vec3(1.0f, 0.75f, 0.25f), 5000.0f);
    m_FXMaterials[FX_DANGER] = Emissive(Vec3(1.0f, 0.15f, 0.1f), 4000.0f);
    m_FXMaterials[FX_HEAL]   = Emissive(Vec3(0.4f, 1.0f, 0.5f), 4000.0f);
    m_FXMaterials[FX_WHITE]  = Emissive(Vec3(0.9f, 0.95f, 1.0f), 9000.0f);
    m_FXMaterials[FX_SMOKE]  = CreateMaterial(Vec3(0.07f, 0.07f, 0.08f), Vec3(0.0f), 1.0f);
    m_FXMaterials[FX_DIRT]   = CreateMaterial(Vec3(0.25f, 0.18f, 0.12f), Vec3(0.0f), 1.0f);
    m_FXMaterials[FX_BONE]   = CreateMaterial(Vec3(0.8f, 0.76f, 0.66f), Vec3(0.0f), 0.8f);
    m_FXMaterials[FX_GOO]    = CreateMaterial(Vec3(0.35f, 0.45f, 0.15f), Vec3(0.2f, 0.4f, 0.05f) * 800.0f, 0.4f);
    m_FXMaterials[FX_FLESH]  = CreateMaterial(Vec3(0.3f, 0.05f, 0.05f), Vec3(0.0f), 0.6f);
}


void DuskSwarmScript::CreateArena()
{
    const Model ground = CreateModel("Ground");
    SetTransform(ground.mRoot, Vec3(0.0f), Vec3(1.0f));

    const auto ground_parts = m_MeshTemplates.find("Ground");

    for (size_t index = 0; ground_parts != m_MeshTemplates.end() && index < ground.mMeshes.size(); index++)
    {
        const String& part = ground_parts->second[index].mName;

        for (int layer = 0; layer < GROUND_LAYER_COUNT; layer++)
        {
            if (part == cGroundLayers[layer].mPart && m_GroundTextures[layer].mAlbedo != 0)
                SetMaterial(ground.mMeshes[index].mNode, m_GroundMaterials[layer]);
        }
    }

    const float wall_offset = cArenaHalfSize + 0.5f;

    Array<Model> walls = CreateModels("WallSegment", 80);

    for (int index = 0; index < 80; index++)
    {
        const int side = index / 20;
        const float along = -cArenaHalfSize + 2.0f + float(index % 20) * 4.0f;

        Vec3 position = Vec3(along, 0.0f, side == 0 ? -wall_offset : wall_offset);
        float yaw = 0.0f;

        if (side >= 2)
        {
            position = Vec3(side == 2 ? -wall_offset : wall_offset, 0.0f, along);
            yaw = glm::half_pi<float>();
        }

        SetTransform(walls[index].mRoot, position, Vec3(1.0f, RandomFloat(0.9f, 1.15f), 1.0f), Quat(Vec3(0.0f, yaw + ( index % 2 ? glm::pi<float>() : 0.0f ), 0.0f)));
    }

    Array<Vec2> lantern_positions;

    for (float offset = -cArenaHalfSize + 5.0f; offset <= cArenaHalfSize - 5.0f; offset += 10.0f)
    {
        for (const Vec2& position : { Vec2(offset, -wall_offset + 1.0f), Vec2(offset, wall_offset - 1.0f), Vec2(-wall_offset + 1.0f, offset), Vec2(wall_offset - 1.0f, offset) })
            lantern_positions.push_back(position);
    }

    Array<Model> lanterns = CreateModels("LanternPost", int(lantern_positions.size()));

    for (size_t index = 0; index < lanterns.size(); index++)
        SetTransform(lanterns[index].mRoot, ToWorld(lantern_positions[index], 0.0f), Vec3(1.0f), Quat(Vec3(0.0f, RandomFloat(-0.2f, 0.2f), 0.0f)));

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

    const auto PlaceGroup = [&](const char* inModel, int inCount, float inRadius, float inScaleMin, float inScaleMax, float inTilt)
    {
        Array<Vec2> positions;
        Vec2 position;

        for (int index = 0; index < inCount; index++)
        {
            if (PlaceObstacle(inRadius, position))
                positions.push_back(position);
        }

        Array<Model> models = CreateModels(inModel, int(positions.size()));

        for (size_t index = 0; index < models.size(); index++)
        {
            const float scale = RandomFloat(inScaleMin, inScaleMax);
            const Quat rotation = Quat(Vec3(RandomFloat(-inTilt, inTilt), RandomFloat(0.0f, glm::two_pi<float>()), RandomFloat(-inTilt, inTilt)));
            SetTransform(models[index].mRoot, ToWorld(positions[index], 0.0f), Vec3(scale), rotation);
        }
    };

    PlaceGroup("Gravestone", 20, 0.45f, 0.9f, 1.1f, 0.08f);
    PlaceGroup("GraveCross", 12, 0.4f, 0.9f, 1.15f, 0.08f);
    PlaceGroup("Rock0", 6, 0.75f, 0.85f, 1.15f, 0.0f);
    PlaceGroup("Rock1", 6, 0.75f, 0.85f, 1.15f, 0.0f);
    PlaceGroup("Rock2", 6, 0.75f, 0.85f, 1.15f, 0.0f);
    PlaceGroup("DeadTree0", 7, 0.4f, 0.85f, 1.25f, 0.04f);
    PlaceGroup("DeadTree1", 7, 0.4f, 0.85f, 1.25f, 0.04f);
    PlaceGroup("Pillar", 8, 0.5f, 0.9f, 1.15f, 0.03f);

    const auto Scatter = [&](const char* inModel, int inCount, float inScaleMin, float inScaleMax)
    {
        Array<Model> models = CreateModels(inModel, inCount);

        for (Model& model : models)
        {
            const Vec2 position = Vec2(RandomFloat(-cArenaHalfSize + 1.0f, cArenaHalfSize - 1.0f), RandomFloat(-cArenaHalfSize + 1.0f, cArenaHalfSize - 1.0f));
            SetTransform(model.mRoot, ToWorld(position, 0.0f), Vec3(RandomFloat(inScaleMin, inScaleMax)), Quat(Vec3(0.0f, RandomFloat(0.0f, glm::two_pi<float>()), 0.0f)));
        }
    };

    Scatter("GrassTuft", 180, 0.8f, 1.4f);
    Scatter("Mushrooms", 30, 0.8f, 1.3f);
    Scatter("Bones", 18, 0.9f, 1.2f);
}


void DuskSwarmScript::CreatePlayer()
{
    m_PlayerEntity = CreateEntity("Player");
    m_PlayerModel = CreateModel("Hero");

    const Entity torch = CreateEntity("Torch", m_PlayerEntity);
    SetTransform(torch, Vec3(0.0f, 3.0f, 0.0f), Vec3(1.0f));

    Light& light = m_Scene->Add<Light>(torch);
    light.type = LIGHT_TYPE_POINT;
    light.color = Vec4(1.0f, 0.72f, 0.45f, 400000.0f);
    light.attributes.x = 12.0f;
}


void DuskSwarmScript::CreatePools()
{
    m_Enemies.clear();

    for (int type = 0; type < ENEMY_TYPE_COUNT; type++)
    {
        const EnemyArchetype& archetype = cEnemyArchetypes[type];
        Array<Model> models = CreateModels(archetype.mName, archetype.mPoolSize);

        for (Model& model : models)
        {
            Enemy& enemy = m_Enemies.emplace_back();
            enemy.mType = type;
            enemy.mModel = model;
            Park(model.mRoot);
        }
    }

    m_Projectiles.resize(cProjectilePoolSize);
    Array<Model> bolts = CreateModels("Bolt", cProjectilePoolSize, m_BoltMaterial);

    for (int index = 0; index < cProjectilePoolSize; index++)
    {
        m_Projectiles[index].mNode = bolts[index].mRoot;
        Park(bolts[index].mRoot);
    }

    m_Fireballs.resize(cFireballPoolSize);
    Array<Model> fireballs = CreateModels("Fireball", cFireballPoolSize);

    for (int index = 0; index < cFireballPoolSize; index++)
    {
        m_Fireballs[index].mNode = fireballs[index].mRoot;
        Park(fireballs[index].mRoot);
    }

    m_Arrows.resize(cArrowPoolSize);
    Array<Model> arrows = CreateModels("Arrow", cArrowPoolSize);

    for (int index = 0; index < cArrowPoolSize; index++)
    {
        m_Arrows[index].mNode = arrows[index].mRoot;
        m_Arrows[index].mRadius = 0.25f;
        Park(arrows[index].mRoot);
    }

    m_Orbs.resize(cOrbPoolSize);
    Array<Model> orbs = CreateModels("Orb", cOrbPoolSize);

    for (int index = 0; index < cOrbPoolSize; index++)
    {
        m_Orbs[index].mNode = orbs[index].mRoot;
        m_Orbs[index].mRadius = 0.35f;
        Park(orbs[index].mRoot);
    }

    m_Meteors.resize(cMeteorPoolSize);
    Array<Model> meteors = CreateModels("Meteor", cMeteorPoolSize);

    for (int index = 0; index < cMeteorPoolSize; index++)
    {
        m_Meteors[index].mNode = meteors[index].mRoot;
        Park(meteors[index].mRoot);
    }

    m_Gems.resize(cGemPoolSize);
    Array<Model> gems = CreateModels("Gem", cGemPoolSize, m_GemMaterials[0]);

    for (int index = 0; index < cGemPoolSize; index++)
    {
        m_Gems[index].mModel = gems[index];
        Park(gems[index].mRoot);
    }

    Array<Model> blades = CreateModels("Blade", cMaxBlades);

    for (int index = 0; index < cMaxBlades; index++)
    {
        m_Blades[index] = blades[index].mRoot;
        Park(m_Blades[index]);
    }

    m_Pickups.clear();

    for (int type = 0; type < PICKUP_COUNT; type++)
    {
        Array<Model> models = CreateModels(cPickupModels[type], cPickupsPerType);

        for (Model& model : models)
        {
            Pickup& pickup = m_Pickups.emplace_back();
            pickup.mType = type;
            pickup.mModel = model;
            Park(model.mRoot);
        }
    }

    Array<Model> pickup_glows = CreateModels("Ring", int(m_Pickups.size()), m_FXMaterials[FX_GOLD]);

    for (size_t index = 0; index < m_Pickups.size(); index++)
    {
        m_Pickups[index].mGlow = pickup_glows[index].mRoot;
        Park(pickup_glows[index].mRoot);
    }

    for (int mesh = 0; mesh < PARTICLE_MESH_COUNT; mesh++)
    {
        m_Particles[mesh].clear();
        m_ParticleCursor[mesh] = 0;

        Array<Model> models = CreateModels(cParticleModels[mesh], cParticlePoolSizes[mesh], m_FXMaterials[FX_SPARK]);

        for (Model& model : models)
        {
            Particle& particle = m_Particles[mesh].emplace_back();
            particle.mNode = model.mRoot;
            particle.mMaterial = m_FXMaterials[FX_SPARK];
            Park(model.mRoot);
        }
    }

    m_NovaBursts.resize(cNovaBurstPoolSize);

    const StaticArray<std::pair<const char*, Vec4>, 3> nova_layers =
    {
        std::pair { "Starburst", Vec4(0.6f, 0.22f, 1.0f, 900.0f) },
        std::pair { "Starburst", Vec4(0.85f, 0.6f, 1.0f, 1400.0f) },
        std::pair { "Disc", Vec4(1.0f, 0.95f, 1.0f, 3000.0f) },
    };

    for (NovaBurst& burst : m_NovaBursts)
    {
        for (size_t layer = 0; layer < nova_layers.size(); layer++)
        {
            const Vec3 color = Vec3(nova_layers[layer].second);
            burst.mMaterials[layer] = CreateMaterial(color, color * nova_layers[layer].second.w, 0.5f);

            if (Material* material = FindComponent<Material>(burst.mMaterials[layer]))
                material->blendMode = MATERIAL_BLEND_MODE_BLENDED;

            burst.mNodes[layer] = CreateModels(nova_layers[layer].first, 1, burst.mMaterials[layer])[0].mRoot;
            Park(burst.mNodes[layer]);
        }
    }

    m_Telegraphs.resize(cTelegraphPoolSize);
    Array<Model> discs = CreateModels("Disc", cTelegraphPoolSize, m_DangerMaterial);
    Array<Model> telegraph_rings = CreateModels("Ring", cTelegraphPoolSize, m_DangerMaterial);

    for (int index = 0; index < cTelegraphPoolSize; index++)
    {
        m_Telegraphs[index].mDisc = discs[index].mRoot;
        m_Telegraphs[index].mRing = telegraph_rings[index].mRoot;
        Park(discs[index].mRoot);
        Park(telegraph_rings[index].mRoot);
    }

    m_Beams.resize(cBeamPoolSize);
    Array<Model> beams = CreateModels("Beam", cBeamPoolSize, m_FXMaterials[FX_WHITE]);

    for (int index = 0; index < cBeamPoolSize; index++)
    {
        m_Beams[index].mNode = beams[index].mRoot;
        Park(beams[index].mRoot);
    }

    m_EliteAuras.resize(cEliteAuraPoolSize);
    Array<Model> auras = CreateModels("Ring", cEliteAuraPoolSize, m_FXMaterials[FX_GOLD]);

    for (int index = 0; index < cEliteAuraPoolSize; index++)
    {
        m_EliteAuras[index].mNode = auras[index].mRoot;
        Park(auras[index].mRoot);
    }

    m_Lights.resize(cLightPoolSize);

    for (FlashLight& flash : m_Lights)
    {
        flash.mEntity = CreateEntity("Flash Light");
        SetTransform(flash.mEntity, cParkedPosition, Vec3(1.0f));

        Light& light = m_Scene->Add<Light>(flash.mEntity);
        light.type = LIGHT_TYPE_POINT;
        light.color = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
        light.attributes.x = 10.0f;
    }
}


void DuskSwarmScript::CreateCamera()
{
    m_CameraEntity = CreateEntity("Swarm Camera");

    Camera& camera = m_Scene->Add<Camera>(m_CameraEntity);
    camera.SetFov(m_FirstPerson ? cFirstPersonFov : cThirdPersonFov);

    m_App->SetCameraEntity(m_CameraEntity);
}

} // RK
