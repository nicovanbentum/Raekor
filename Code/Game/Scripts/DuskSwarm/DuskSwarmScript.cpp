#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::OnStart()
{
    DestroyRuntimeEntities();

    m_Random.seed(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

    m_RootEntity = m_Scene->CreateSpatialEntity(cRuntimeRootName);
    m_Nodes.Init(m_Scene, m_App->GetRenderInterface(), m_RootEntity);

    m_GodMode = &g_CVariables->Create("ds_god", 0, true);
    m_AutoPick = &g_CVariables->Create("ds_autopick", 0, true);
    m_StartTime = &g_CVariables->Create("ds_start_time", 0.0f, true);
    m_StartWeapons = &g_CVariables->Create("ds_weapons", 0, true);
    m_ShakeScale = &g_CVariables->Create("ds_shake", 1.0f, true);
    m_SensitivityScale = &g_CVariables->Create("ds_mouse_sens", 1.0f, true);

    LoadTextures();
    LoadModels();
    CreateMaterials();
    CreateArena();
    CreatePlayer();
    CreatePools();
    CreateCamera();
    CreateAudio();
    CreateSky();

    NewGame();
    UpdateSky();

    m_Started = true;
}


void DuskSwarmScript::OnStop()
{
    RestoreSky();
    DestroyRuntimeEntities();

    m_Started = false;
}


void DuskSwarmScript::OnUpdate(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    if (!m_Started)
        OnStart();

    float delta_time = glm::min(inDeltaTime, 0.05f);

    m_Time += delta_time;
    m_PauseTime = 0.0f;
    m_DraggingSlider = SLIDER_NONE;

    if (m_HitStop > 0.0f)
    {
        m_HitStop -= delta_time;
        delta_time *= 0.08f;
    }

    if (m_State == STATE_LEVEL_UP && *m_AutoPick && m_Time - m_LevelUpTime > 1.5f)
        ChooseUpgrade(RandomInt(0, m_ChoiceCount - 1));

    if (m_State == STATE_PLAYING)
    {
        m_RunTime += delta_time;

        UpdatePlayer(delta_time);
        UpdateSpawning(delta_time);
        BuildGrid();
        UpdateEnemies(delta_time);
        UpdateWeapons(delta_time);
        UpdateProjectiles(delta_time);
        UpdateFireballs(delta_time);
        UpdateMeteors(delta_time);
        UpdateEnemyProjectiles(delta_time);
        UpdateGems(delta_time);
        UpdatePickups(delta_time);
        UpdateProgress();
    }
    else if (m_State == STATE_GAME_OVER || m_State == STATE_VICTORY)
    {
        m_PlayerDeathTimer += delta_time;
        UpdateJump(delta_time);
        BuildGrid();
    }

    if (m_State != STATE_LEVEL_UP)
    {
        UpdateDying(delta_time);
        UpdateParticles(delta_time);
        UpdateNovaBursts(delta_time);
        UpdateTelegraphs(delta_time);
        UpdateBeams(delta_time);
        UpdateLights(delta_time);
        UpdateVisuals(delta_time);
    }

    UpdateCamera(inDeltaTime);
    UpdateWand();
    UpdateSky();
    UpdateSounds();
    m_Nodes.Update();
    DrawHUD(inDeltaTime);
}


void DuskSwarmScript::OnEvent(const SDL_Event& inEvent)
{
    if (!m_Started)
        return;

    if (inEvent.type == SDL_EVENT_MOUSE_WHEEL && !m_FirstPerson)
        m_CameraDistance = glm::clamp(m_CameraDistance - inEvent.wheel.y * 1.5f, 14.0f, 34.0f);

    if (inEvent.type == SDL_EVENT_MOUSE_MOTION && m_FirstPerson && m_State == STATE_PLAYING)
    {
        const float sensitivity = cMouseSensitivity * GetSensitivityScale();
        m_LookYaw -= inEvent.motion.xrel * sensitivity;
        m_LookPitch = glm::clamp(m_LookPitch - inEvent.motion.yrel * sensitivity, -cMaxLookPitch, cMaxLookPitch);
    }

    if (inEvent.type != SDL_EVENT_KEY_DOWN || inEvent.key.repeat)
        return;

    const SDL_Keycode key = inEvent.key.key;

    if (key == SDLK_T)
    {
        ToggleFirstPerson();
        return;
    }

    if (m_State == STATE_GAME_OVER || m_State == STATE_VICTORY)
    {
        if (key == SDLK_RETURN && m_PlayerDeathTimer > 1.0f)
            NewGame();

        return;
    }

    if (m_State == STATE_PLAYING)
    {
        if (key == SDLK_SPACE)
            TryJump();
        else if (key == SDLK_LSHIFT)
            TryDash();

        return;
    }

    if (m_State != STATE_LEVEL_UP || m_ChoiceCount == 0 || m_Time - m_LevelUpTime < 0.35f)
        return;

    switch (key)
    {
        case SDLK_1: ChooseUpgrade(0); break;
        case SDLK_2: ChooseUpgrade(1); break;
        case SDLK_3: ChooseUpgrade(2); break;
        case SDLK_LEFT:
        case SDLK_A: m_SelectedChoice = ( m_SelectedChoice + m_ChoiceCount - 1 ) % m_ChoiceCount; PlaySfx(cSelectSound, 0.5f); break;
        case SDLK_RIGHT:
        case SDLK_D: m_SelectedChoice = ( m_SelectedChoice + 1 ) % m_ChoiceCount; PlaySfx(cSelectSound, 0.5f); break;
        case SDLK_SPACE:
        case SDLK_RETURN: ChooseUpgrade(m_SelectedChoice); break;
    }
}


void DuskSwarmScript::OnPausedUpdate(float inDeltaTime)
{
    if (!m_Started)
        return;

    m_PauseTime += inDeltaTime;

    DrawHUD(0.0f);
    DrawPauseMenu();
}


void DuskSwarmScript::OnPausedEvent(const SDL_Event& inEvent)
{
    if (!m_Started)
        return;

    const PauseLayout layout = GetPauseLayout();

    switch (inEvent.type)
    {
        case SDL_EVENT_MOUSE_MOTION:
        {
            m_MousePosition = Vec2(inEvent.motion.x, inEvent.motion.y);

            if (m_DraggingSlider != SLIDER_NONE)
                SetSliderFromMouse(layout, m_DraggingSlider);
        } break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (inEvent.button.button != SDL_BUTTON_LEFT)
                break;

            m_MousePosition = Vec2(inEvent.button.x, inEvent.button.y);

            for (int tab = 0; tab < PAUSE_TAB_COUNT; tab++)
            {
                if (IsInside(m_MousePosition, layout.mTabPos[tab], layout.mTabSize))
                    SetPauseTab(tab);
            }

            if (m_PauseTab == PAUSE_TAB_GENERAL)
            {
                for (int slider = 0; slider < SLIDER_COUNT; slider++)
                {
                    if (IsInside(m_MousePosition, layout.GetSliderPos(slider) - layout.mSliderPadding, layout.mSliderSize + layout.mSliderPadding * 2.0f))
                    {
                        m_DraggingSlider = slider;
                        SetSliderFromMouse(layout, slider);
                    }
                }

                if (IsInside(m_MousePosition, layout.mGodModePos, layout.mCheckboxSize))
                    ToggleGodMode();
            }
            else
            {
                for (int setting = 0; setting < int(cGraphicsSettings.size()); setting++)
                {
                    if (IsInside(m_MousePosition, layout.GetSettingPos(setting), layout.mCheckboxSize))
                        ToggleGraphicsSetting(setting);
                }
            }

            if (IsInside(m_MousePosition, layout.mButtonPos, layout.mButtonSize))
                Resume();
            else if (IsInside(m_MousePosition, layout.mQuitPos, layout.mButtonSize))
                Quit();
        } break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (inEvent.button.button == SDL_BUTTON_LEFT)
                m_DraggingSlider = SLIDER_NONE;
        } break;

        case SDL_EVENT_KEY_DOWN:
        {
            switch (inEvent.key.key)
            {
                case SDLK_LEFT:
                case SDLK_A: SetMasterVolume(GetMasterVolume() - cVolumeStep); break;
                case SDLK_RIGHT:
                case SDLK_D: SetMasterVolume(GetMasterVolume() + cVolumeStep); break;
                case SDLK_RETURN:
                case SDLK_SPACE: if (!inEvent.key.repeat) Resume(); break;
                case SDLK_Q: if (!inEvent.key.repeat) Quit(); break;
                case SDLK_G: if (!inEvent.key.repeat) ToggleGodMode(); break;
                case SDLK_TAB: if (!inEvent.key.repeat) SetPauseTab(( m_PauseTab + 1 ) % PAUSE_TAB_COUNT); break;
            }
        } break;
    }
}


Entity DuskSwarmScript::CreateEntity(StringView inName, Entity inParent)
{
    const Entity entity = m_Scene->CreateSpatialEntity(inName);
    m_Scene->ParentTo(entity, inParent != Entity::Null ? inParent : m_RootEntity);
    return entity;
}


Entity DuskSwarmScript::CreateMaterial(Vec3 inAlbedo, Vec3 inEmissive, float inRoughness, float inMetallic)
{
    const Entity entity = CreateEntity("Material");

    Material& material = m_Scene->Add<Material>(entity);
    material.albedo = Vec4(inAlbedo, 1.0f);
    material.emissive = inEmissive;
    material.roughness = inRoughness;
    material.metallic = inMetallic;

    return entity;
}


void DuskSwarmScript::SetTransform(Entity inEntity, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation)
{
    if (Transform* transform = FindComponent<Transform>(inEntity))
    {
        transform->position = inPosition;
        transform->rotation = inRotation;
        transform->scale = inScale;
        transform->Compose();
    }
}


void DuskSwarmScript::SetTransform(Node inNode, const Vec3& inPosition, const Vec3& inScale, const Quat& inRotation)
{
    m_Nodes.SetTransform(inNode, inPosition, inScale, inRotation);
}


void DuskSwarmScript::Park(Node inNode)
{
    m_Nodes.Park(inNode);
}


void DuskSwarmScript::SetMaterial(Node inNode, Entity inMaterial)
{
    m_Nodes.SetMaterial(inNode, inMaterial);
}


void DuskSwarmScript::SetModelMaterial(const Model& inModel, Entity inMaterial)
{
    for (const ModelMesh& mesh : inModel.mMeshes)
        SetMaterial(mesh.mNode, inMaterial);
}


void DuskSwarmScript::RestoreModelMaterials(const Model& inModel)
{
    for (const ModelMesh& mesh : inModel.mMeshes)
        SetMaterial(mesh.mNode, mesh.mMaterial);
}


void DuskSwarmScript::Pose(const Model& inModel, int inPart, const Vec3& inOffset, const Vec3& inEuler, float inScale)
{
    const RigPart& part = inModel.mParts[inPart];

    if (part.mNode != Node::Null)
        SetTransform(part.mNode, part.mRest + inOffset, Vec3(inScale), Quat(inEuler));
}


float DuskSwarmScript::RandomFloat(float inMin, float inMax)
{
    return std::uniform_real_distribution<float>(inMin, inMax)(m_Random);
}


int DuskSwarmScript::RandomInt(int inMin, int inMax)
{
    return std::uniform_int_distribution<int>(inMin, inMax)(m_Random);
}


Vec3 DuskSwarmScript::RandomDirection()
{
    const float z = RandomFloat(-1.0f, 1.0f);
    const float angle = RandomFloat(0.0f, glm::two_pi<float>());
    const float r = glm::sqrt(glm::max(0.0f, 1.0f - z * z));
    return Vec3(glm::cos(angle) * r, z, glm::sin(angle) * r);
}


Vec2 DuskSwarmScript::RandomCircle()
{
    const float angle = RandomFloat(0.0f, glm::two_pi<float>());
    return Vec2(glm::cos(angle), glm::sin(angle));
}


Vec3 DuskSwarmScript::ToWorld(const Vec2& inPosition, float inHeight)
{
    return Vec3(inPosition.x, inHeight, inPosition.y);
}


float DuskSwarmScript::GetYaw(const Vec2& inDirection)
{
    return glm::atan(-inDirection.x, -inDirection.y);
}


float DuskSwarmScript::LerpAngle(float inFrom, float inTo, float inT)
{
    const float delta = glm::mod(inTo - inFrom + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
    return inFrom + delta * inT;
}


Quat DuskSwarmScript::LookRotation(const Vec3& inDirection)
{
    const float length = glm::length(inDirection);

    if (length < 1e-5f)
        return Quat(1.0f, 0.0f, 0.0f, 0.0f);

    const Vec3 direction = inDirection / length;
    const Vec3 up = glm::abs(direction.y) > 0.98f ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);
    return glm::quatLookAtRH(direction, up);
}


float DuskSwarmScript::EaseOutCubic(float inT)
{
    const float t = 1.0f - glm::clamp(inT, 0.0f, 1.0f);
    return 1.0f - t * t * t;
}


float DuskSwarmScript::EaseOutBack(float inT)
{
    const float t = glm::clamp(inT, 0.0f, 1.0f) - 1.0f;
    return 1.0f + 2.7f * t * t * t + 1.7f * t * t;
}


Vec2 DuskSwarmScript::ClampToArena(const Vec2& inPosition, float inRadius)
{
    return glm::clamp(inPosition, Vec2(-cArenaHalfSize + inRadius), Vec2(cArenaHalfSize - inRadius));
}


int DuskSwarmScript::GetRigPart(const String& inName)
{
    static const HashMap<String, int> cParts =
    {
        { "Body", RIG_BODY }, { "Main", RIG_BODY }, { "Base", RIG_BODY }, { "Head", RIG_HEAD }, { "Lid", RIG_HEAD },
        { "ArmL", RIG_ARM_L }, { "ArmR", RIG_ARM_R }, { "LegL", RIG_LEG_L }, { "LegR", RIG_LEG_R }, { "WingL", RIG_WING_L }, { "WingR", RIG_WING_R },
    };

    const auto part = cParts.find(inName);
    return part != cParts.end() ? part->second : -1;
}


void DuskSwarmScript::DestroyRuntimeEntities()
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
    m_ListenerEntity = Entity::Null;
    m_MusicEntity = Entity::Null;
    m_BossMusicEntity = Entity::Null;
    m_PlayerModel = Model();
    m_WandModel = Model();
    m_Blades.fill(Node::Null);
    m_Nodes.Clear();
    m_Enemies.clear();
    m_Projectiles.clear();
    m_Fireballs.clear();
    m_Arrows.clear();
    m_Orbs.clear();
    m_Meteors.clear();
    m_Gems.clear();
    m_Pickups.clear();
    m_NovaBursts.clear();
    m_Telegraphs.clear();
    m_Beams.clear();
    m_Lights.clear();
    m_EliteAuras.clear();
    m_Obstacles.clear();

    for (Array<Particle>& particles : m_Particles)
        particles.clear();

    ReleaseModels();
}


void DuskSwarmScript::NewGame()
{
    for (Enemy& enemy : m_Enemies)
    {
        enemy.mActive = false;
        enemy.mDying = false;
        enemy.mAura = -1;
        enemy.mTelegraph = -1;
        RestoreModelMaterials(enemy.mModel);
        Park(enemy.mModel.mRoot);
    }

    for (Projectile& projectile : m_Projectiles)
    {
        projectile.mActive = false;
        Park(projectile.mNode);
    }

    for (Fireball& fireball : m_Fireballs)
    {
        fireball.mActive = false;
        Park(fireball.mNode);
    }

    for (Array<EnemyProjectile>* pool : { &m_Arrows, &m_Orbs })
    {
        for (EnemyProjectile& projectile : *pool)
        {
            projectile.mActive = false;
            Park(projectile.mNode);
        }
    }

    for (Meteor& meteor : m_Meteors)
    {
        meteor.mActive = false;
        Park(meteor.mNode);
    }

    for (Gem& gem : m_Gems)
    {
        gem.mActive = false;
        Park(gem.mModel.mRoot);
    }

    for (Pickup& pickup : m_Pickups)
    {
        pickup.mActive = false;
        Park(pickup.mModel.mRoot);
        Park(pickup.mGlow);
    }

    for (Array<Particle>& particles : m_Particles)
    {
        for (Particle& particle : particles)
        {
            particle.mActive = false;
            Park(particle.mNode);
        }
    }

    for (NovaBurst& burst : m_NovaBursts)
    {
        burst.mActive = false;

        for (Node node : burst.mNodes)
            Park(node);
    }

    for (Telegraph& telegraph : m_Telegraphs)
        ReleaseTelegraph(telegraph);

    for (Beam& beam : m_Beams)
    {
        beam.mActive = false;
        Park(beam.mNode);
    }

    for (EliteAura& aura : m_EliteAuras)
    {
        aura.mActive = false;
        Park(aura.mNode);
    }

    for (FlashLight& flash : m_Lights)
    {
        flash.mActive = false;

        if (Light* light = FindComponent<Light>(flash.mEntity))
            light->color.a = 0.0f;
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

    for (Node blade : m_Blades)
        Park(blade);

    m_UpgradeLevels.fill(0);
    m_UpgradeLevels[UPGRADE_BOLT] = 1;

    m_State = STATE_PLAYING;
    m_RunTime = 0.0f;
    m_PlayerPosition = Vec2(0.0f);
    m_PlayerVelocity = Vec2(0.0f);
    m_PlayerYaw = 0.0f;
    m_PlayerWalkPhase = 0.0f;
    m_PlayerDeathTimer = 0.0f;
    m_Health = cPlayerBaseHealth;
    m_InvulnerableTimer = 0.0f;
    m_DashTimer = -1.0f;
    m_DashCooldownTimer = 0.0f;
    m_JumpHeight = 0.0f;
    m_JumpVelocity = 0.0f;
    m_CastTimer = 0.0f;
    m_HitFlash = 0.0f;
    m_ScreenFlash = 0.0f;
    m_Trauma = 0.0f;
    m_HitStop = 0.0f;
    m_CameraKick = 0.0f;
    m_Level = 1;
    m_Experience = 0;
    m_PendingLevelUps = 0;
    m_Kills = 0;
    m_EliteKills = 0;
    m_BossKills = 0;
    m_Streak = 0;
    m_BestStreak = 0;
    m_StreakTimer = 0.0f;
    m_DamageDealt = 0.0f;
    m_SpawnAccumulator = 0.0f;
    m_NextSwarmTime = cSwarmInterval;
    m_NextBruteTime = cBruteInterval;
    m_NextBoss = 0;
    m_BossEnemy = -1;
    m_BossPattern = 0;
    m_BoltTimer = 0.5f;
    m_NovaTimer = 2.0f;
    m_FireballTimer = 1.0f;
    m_LightningTimer = 1.0f;
    m_MeteorTimer = 2.0f;
    m_BladeAngle = 0.0f;
    m_EndTime = 0.0f;
    m_ChoiceCount = 0;
    m_DamageNumbers.clear();
    m_Popups.clear();
    m_XPDrops.clear();
    m_LichKills = 0;
    m_ChatMessages.clear();

    AddChat("Welcome to Dusk Swarm.", cChatBlack);
    AddChat("WASD to move, Space to jump, Shift to dash.", cChatBlue);
    AddChat("Your weapons fire on their own.", cChatBlue);
    AddChat("Press T to toggle first person.", cChatBlue);
    AddChat("Survive three nights until the final dawn.", cChatRed);
    m_CameraTarget = Vec3(0.0f);
    m_FacingDirection = Vec2(0.0f, -1.0f);
    m_GemStreak = 0;
    m_LastGemTime = -cGemStreakTime;
    m_EndJinglePlayed = false;

    PlayMusic(false);

    if (*m_StartWeapons > 0)
    {
        for (int upgrade = UPGRADE_BOLT; upgrade <= UPGRADE_METEOR; upgrade++)
            m_UpgradeLevels[upgrade] = glm::clamp(*m_StartWeapons, 1, cMaxUpgradeLevel);
    }

    if (*m_StartTime > 0.0f)
    {
        m_RunTime = *m_StartTime;

        while (m_NextSwarmTime <= m_RunTime)
            m_NextSwarmTime += cSwarmInterval;

        while (m_NextBruteTime <= m_RunTime)
            m_NextBruteTime += cBruteInterval;

        while (m_NextBoss < int(cBossTimes.size()) && cBossTimes[m_NextBoss] < m_RunTime)
            m_NextBoss++;
    }
}


RTTI_DEFINE_TYPE(DuskSwarmScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(DuskSwarmScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(DuskSwarmScript, SERIALIZE_ALL, "Best Time", m_BestTime);
    RTTI_DEFINE_SCRIPT_MEMBER(DuskSwarmScript, SERIALIZE_ALL, "Camera Distance", m_CameraDistance);
}

RK_REGISTER_SCRIPT(DuskSwarmScript)

} // RK
