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
}

} // RK
