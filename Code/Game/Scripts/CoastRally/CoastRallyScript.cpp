#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

void CoastRallyScript::OnStart()
{
    DestroyRuntimeEntities();

    m_Random.seed(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()));

    m_RootEntity = m_Scene->CreateSpatialEntity(cRuntimeRootName);
    m_Nodes.Init(m_Scene, m_App->GetRenderInterface(), m_RootEntity);

    m_AutoDrive = &g_CVariables->Create("cr_autodrive", 0, true);

    LoadTextures();
    LoadModels();
    CreateMaterials();
    CreateLevel();
    CreateCar();
    CreateProps();
    CreateCamera();
    CreateAudio();

    NewGame();

    m_Started = true;
}


void CoastRallyScript::OnStop()
{
    DestroyRuntimeEntities();

    m_Started = false;
}


void CoastRallyScript::OnUpdate(float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    if (!m_Started)
        OnStart();

    const float delta_time = glm::min(inDeltaTime, 0.05f);

    m_Time += delta_time;
    m_DriveTime += delta_time;

    UpdateCar(delta_time);
    UpdateProps();
    UpdateCarVisuals();
    UpdateCamera(delta_time);
    UpdateSounds(delta_time);
    m_Nodes.Update();
    DrawHUD(delta_time);
}


void CoastRallyScript::OnEvent(const SDL_Event& inEvent)
{
    if (!m_Started)
        return;

    if (inEvent.type == SDL_EVENT_MOUSE_MOTION && m_CameraMode != CAMERA_HOOD)
    {
        m_OrbitYaw -= inEvent.motion.xrel * cMouseOrbitSpeed;
        m_OrbitPitch = glm::clamp(m_OrbitPitch + inEvent.motion.yrel * cMouseOrbitSpeed, -0.25f, 0.9f);
        m_OrbitIdle = 0.0f;
    }

    if (inEvent.type != SDL_EVENT_KEY_DOWN || inEvent.key.repeat)
        return;

    switch (inEvent.key.key)
    {
        case SDLK_R: ResetCar(); break;
        case SDLK_C: SetCameraMode(( m_CameraMode + 1 ) % CAMERA_MODE_COUNT); break;
        case SDLK_H: m_ShowControls = !m_ShowControls; break;
        case SDLK_BACKSPACE: NewGame(); break;
    }
}


void CoastRallyScript::OnPausedUpdate(float inDeltaTime)
{
    if (!m_Started)
        return;

    m_Nodes.Update();
    DrawHUD(0.0f);

    const Vec2 display = Vec2(m_App->GetViewport().GetDisplaySize());
    const float scale = glm::max(display.y / 1080.0f, 0.5f);

    g_UIRenderer.AddRectFilled(Vec2(0.0f), display, 0.0f, Vec4(0.0f, 0.02f, 0.05f, 0.45f));
    g_UIRenderer.SetFont(m_HudFont);
    DrawText(Vec2(display.x * 0.5f, display.y * 0.42f), "PAUSED", 110.0f * scale, cSun, UI_TEXT_ALIGN_CENTER);
    g_UIRenderer.SetFont(m_LabelFont);
    g_UIRenderer.AddText(Vec2(display.x * 0.5f, display.y * 0.42f + 130.0f * scale), "Press Esc to keep driving", 26.0f * scale, cCream, UI_TEXT_ALIGN_CENTER);
}


Entity CoastRallyScript::CreateEntity(StringView inName, Entity inParent)
{
    const Entity entity = m_Scene->CreateSpatialEntity(inName);
    m_Scene->ParentTo(entity, inParent != Entity::Null ? inParent : m_RootEntity);
    return entity;
}


Entity CoastRallyScript::CreateMaterial(Vec3 inAlbedo, Vec3 inEmissive, float inRoughness, float inMetallic)
{
    const Entity entity = CreateEntity("Material");

    Material& material = m_Scene->Add<Material>(entity);
    material.albedo = Vec4(inAlbedo, 1.0f);
    material.emissive = inEmissive;
    material.roughness = inRoughness;
    material.metallic = inMetallic;

    return entity;
}


void CoastRallyScript::SetTransform(Entity inEntity, const Vec3& inPosition, const Quat& inRotation)
{
    if (Transform* transform = FindComponent<Transform>(inEntity))
    {
        transform->position = inPosition;
        transform->rotation = inRotation;
        transform->Compose();
    }
}


void CoastRallyScript::SetTransform(Node inNode, const Vec3& inPosition, const Quat& inRotation, const Vec3& inScale)
{
    m_Nodes.SetTransform(inNode, inPosition, inScale, inRotation);
}


void CoastRallyScript::Park(Node inNode)
{
    m_Nodes.Park(inNode);
}


void CoastRallyScript::SetMaterial(Node inNode, Entity inMaterial)
{
    m_Nodes.SetMaterial(inNode, inMaterial);
}


CoastRallyScript::Node CoastRallyScript::FindPart(const Model& inModel, StringView inName)
{
    for (const ModelPart& part : inModel.mParts)
    {
        if (part.mName == inName)
            return part.mNode;
    }

    return Node::Null;
}


Quat CoastRallyScript::YawRotation(float inYaw)
{
    return glm::angleAxis(inYaw, Vec3(0.0f, 1.0f, 0.0f));
}


float CoastRallyScript::GetYaw(const Vec3& inDirection)
{
    return glm::atan(inDirection.x, inDirection.z);
}


float CoastRallyScript::LerpAngle(float inFrom, float inTo, float inT)
{
    const float delta = glm::mod(inTo - inFrom + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
    return inFrom + delta * inT;
}


Vec3 CoastRallyScript::ToVec3(JPH::Vec3Arg inVector)
{
    return Vec3(inVector.GetX(), inVector.GetY(), inVector.GetZ());
}


Quat CoastRallyScript::ToQuat(JPH::QuatArg inQuat)
{
    return Quat(inQuat.GetW(), inQuat.GetX(), inQuat.GetY(), inQuat.GetZ());
}


JPH::Vec3 CoastRallyScript::ToJolt(const Vec3& inVector)
{
    return JPH::Vec3(inVector.x, inVector.y, inVector.z);
}


JPH::Quat CoastRallyScript::ToJolt(const Quat& inQuat)
{
    return JPH::Quat(inQuat.x, inQuat.y, inQuat.z, inQuat.w).Normalized();
}


JPH::PhysicsSystem* CoastRallyScript::GetPhysicsSystem()
{
    return GetPhysics()->GetSystem();
}


JPH::BodyInterface& CoastRallyScript::GetBodyInterface()
{
    return GetPhysicsSystem()->GetBodyInterface();
}


void CoastRallyScript::DestroyRuntimeEntities()
{
    DestroyCar();
    DestroyProps();
    DestroyLevelBody();

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
    m_EngineEntity = Entity::Null;
    m_SkidEntity = Entity::Null;
    m_WindEntity = Entity::Null;
    m_AmbienceEntity = Entity::Null;
    m_CarModel = Model();
    m_BrakeNode = Node::Null;
    m_WheelNodes.fill(Node::Null);
    m_Placements.clear();
    m_PropPlacements.clear();
    m_Waypoints.clear();
    m_Nodes.Clear();

    ReleaseModels();
}


void CoastRallyScript::NewGame()
{
    ResetProps();
    PlaceCar(m_Spawn.mPosition, m_Spawn.mRotation * Vec3(0.0f, 0.0f, 1.0f));

    m_DriveTime = 0.0f;
    m_Odometer = 0.0f;
    m_TopSpeed = 0.0f;
    m_BestAirTime = 0.0f;
    m_PropsKnocked = 0;
    m_OrbitYaw = 0.0f;
    m_OrbitPitch = 0.0f;
    m_Popups.clear();

    PlayEntitySound(m_EngineEntity);
    PlayEntitySound(m_SkidEntity);
    PlayEntitySound(m_WindEntity);
    PlayEntitySound(m_AmbienceEntity);
}


RTTI_DEFINE_TYPE(CoastRallyScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(CoastRallyScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(CoastRallyScript, SERIALIZE_ALL, "Camera Mode", m_CameraMode);
}

RK_REGISTER_SCRIPT(CoastRallyScript)

} // RK
