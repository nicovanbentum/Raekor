#define RAEKOR_SCRIPT
#include "CoastRallyScript.h"

namespace RK {

void CoastRallyScript::CreateSky()
{
    m_SunEntity = Entity::Null;
    m_RestoreSun = false;

    for (const auto& [entity, light] : m_Scene->Each<DirectionalLight>())
    {
        m_SunEntity = entity;
        m_RestoreSun = true;
        m_SavedSun = light;
        break;
    }

    if (m_SunEntity == Entity::Null)
    {
        m_SunEntity = CreateEntity("Sun");
        m_Scene->Add<DirectionalLight>(m_SunEntity);
    }

    if (DirectionalLight* light = FindComponent<DirectionalLight>(m_SunEntity))
    {
        light->fogStart = cFogStart;
        light->fogEnd = cFogEnd;
        light->fogOpacity = cFogOpacity;
        light->groundAlbedo = cGroundAlbedo;
        light->groundBounce = cGroundBounce;
        light->skyVisibilityMap = m_SkyVisibilityTexture;
        light->groundAlbedoMap = m_GroundAlbedoTexture;
        light->skyVisibilityBounds = Vec4(-cSkyVisibilityHalfSize, -cSkyVisibilityHalfSize, cSkyVisibilityHalfSize * 2.0f, cSkyVisibilityHalfSize * 2.0f);
        light->skyVisibilityHeights = Vec2(cSkyVisibilityHeightMin, cSkyVisibilityHeightRange);
        light->skyVisibilityStrength = cSkyVisibilityStrength;
    }

    if (const Transform* transform = FindComponent<Transform>(m_SunEntity))
        m_SavedSunRotation = transform->rotation;

    m_DayLength = &g_CVariables->Create("cr_day_length", cDayLength, true);
    m_TimeOfDay = glm::fract(g_CVariables->Create("cr_start_time", cStartTimeOfDay, true));

    UpdateSky(0.0f);
}


void CoastRallyScript::RestoreSky()
{
    if (m_RestoreSun)
    {
        if (DirectionalLight* light = FindComponent<DirectionalLight>(m_SunEntity))
            *light = m_SavedSun;

        if (Transform* transform = FindComponent<Transform>(m_SunEntity))
        {
            transform->rotation = m_SavedSunRotation;
            transform->Compose();
        }
    }

    m_SunEntity = Entity::Null;
    m_RestoreSun = false;
}


void CoastRallyScript::UpdateSky(float inDeltaTime)
{
    if (m_DayLength != nullptr && *m_DayLength > 0.0f)
        m_TimeOfDay = glm::fract(m_TimeOfDay + inDeltaTime / *m_DayLength);

    const float angle = ( m_TimeOfDay - 0.25f ) * glm::two_pi<float>();
    const Vec3 sun = Vec3(glm::cos(angle), glm::sin(angle) * glm::cos(cSkyInclination), glm::sin(angle) * glm::sin(cSkyInclination));
    const bool day = sun.y >= 0.0f;
    Vec3 position = day ? sun : -sun + Vec3(0.0f, cMoonLift, 0.0f);
    position.y = glm::max(position.y, cMinSkyHeight);
    position = glm::normalize(position);
    const float height = glm::smoothstep(0.0f, cSkyFadeHeight, position.y);
    const Vec3 color = day ? glm::mix(cSunsetColor, cDaySunColor, glm::smoothstep(0.0f, 0.6f, sun.y)) : cMoonColor;
    const Vec3 direction = -position;

    if (DirectionalLight* light = FindComponent<DirectionalLight>(m_SunEntity))
    {
        light->color = Vec4(color * glm::mix(cHorizonSkyBrightness, 1.0f, height), light->color.a);
        light->illuminance = ( day ? cSunIlluminance : cMoonIlluminance ) * glm::mix(cHorizonIlluminance, 1.0f, height);
        light->direction = Vec4(direction, 0.0f);

        if (Transform* transform = FindComponent<Transform>(m_SunEntity))
        {
            transform->rotation = glm::rotation(glm::normalize(DirectionalLight::cDefaultDirection), direction);
            transform->Compose();
        }
    }

    const float night = 1.0f - glm::smoothstep(-0.05f, 0.15f, sun.y);

    for (Entity entity : m_HeadlightEntities)
    {
        if (Light* light = FindComponent<Light>(entity))
            light->color.a = cHeadlightIntensity * night;
    }

    UpdateNightLights(inDeltaTime, night);
}


void CoastRallyScript::CreateNightLights()
{
    auto CreateSpotLight = [this](StringView inName, const Vec3& inColor, float inRange, float inInnerAngle, float inOuterAngle)
    {
        const Entity entity = CreateEntity(inName);

        Light& light = m_Scene->Add<Light>(entity);
        light.type = LIGHT_TYPE_SPOT;
        light.color = Vec4(inColor, 0.0f);
        light.attributes = Vec4(inRange, inInnerAngle, inOuterAngle, 0.0f);

        return entity;
    };

    m_StreetLampEntities.clear();

    for (const Placement& placement : m_Placements)
    {
        if (placement.mAsset == cStreetLampAsset)
        {
            const Entity entity = CreateSpotLight("Street Lamp", cStreetLampColor, cStreetLampRange, cStreetLampInnerAngle, cStreetLampOuterAngle);
            SetTransform(entity, placement.mPosition + placement.mRotation * ( cStreetLampOffset * placement.mScale ), glm::quatLookAtRH(Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)));
            m_StreetLampEntities.push_back(entity);
        }
        else if (placement.mAsset == cLighthouseAsset && m_LighthouseBeamNode == Node::Null)
        {
            m_LighthouseLampPosition = placement.mPosition + placement.mRotation * ( Vec3(0.0f, cLighthouseLampHeight, 0.0f) * placement.mScale );
            m_LighthouseBeamNode = CreateModel(cLighthouseBeamAsset, m_BeamMaterial).mRoot;

            for (Entity& entity : m_LighthouseLightEntities)
                entity = CreateSpotLight("Lighthouse Beam", cLighthouseColor, cLighthouseRange, cLighthouseInnerAngle, cLighthouseOuterAngle);
        }
    }

    UpdateNightLights(0.0f, 0.0f);
}


void CoastRallyScript::UpdateNightLights(float inDeltaTime, float inNight)
{
    if (Material* palette = FindComponent<Material>(m_PaletteMaterial))
        palette->emissive = m_PaletteEmissiveTexture != 0 ? Vec3(cPaletteNightEmissive * inNight) : Vec3(0.0f);

    for (Entity entity : m_StreetLampEntities)
    {
        if (Light* light = FindComponent<Light>(entity))
            light->color.a = cStreetLampIntensity * inNight;
    }

    if (m_LighthouseBeamNode == Node::Null)
        return;

    m_LighthouseYaw = glm::mod(m_LighthouseYaw + inDeltaTime * cLighthouseSpinSpeed, glm::two_pi<float>());
    const Quat spin = YawRotation(m_LighthouseYaw);

    if (inNight > 0.0f)
        SetTransform(m_LighthouseBeamNode, m_LighthouseLampPosition, spin);
    else
        Park(m_LighthouseBeamNode);

    for (size_t index = 0; index < m_LighthouseLightEntities.size(); index++)
    {
        const float side = index == 0 ? 1.0f : -1.0f;
        const Vec3 direction = spin * Vec3(0.0f, -glm::sin(cLighthouseBeamPitch), side * glm::cos(cLighthouseBeamPitch));

        SetTransform(m_LighthouseLightEntities[index], m_LighthouseLampPosition, glm::quatLookAtRH(direction, Vec3(0.0f, 1.0f, 0.0f)));

        if (Light* light = FindComponent<Light>(m_LighthouseLightEntities[index]))
            light->color.a = cLighthouseIntensity * inNight;
    }

    if (Material* beam = FindComponent<Material>(m_BeamMaterial))
    {
        beam->emissive = cLighthouseColor * cLighthouseBeamEmissive * inNight;
        beam->albedo.a = cLighthouseBeamOpacity * inNight;
    }
}

} // RK
