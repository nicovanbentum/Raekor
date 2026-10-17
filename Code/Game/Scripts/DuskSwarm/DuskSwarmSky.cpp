#define RAEKOR_SCRIPT
#include "DuskSwarmScript.h"

namespace RK {

void DuskSwarmScript::CreateSky()
{
    m_SunEntity = Entity::Null;
    m_RestoreSun = false;

    for (const auto& [entity, light] : m_Scene->Each<DirectionalLight>())
    {
        m_SunEntity = entity;
        m_RestoreSun = true;
        m_SavedSunColor = light.color;
        m_SavedSunIlluminance = light.illuminance;
        break;
    }

    if (m_SunEntity == Entity::Null)
    {
        m_SunEntity = CreateEntity("Sun");
        m_Scene->Add<DirectionalLight>(m_SunEntity);
    }

    if (const Transform* transform = FindComponent<Transform>(m_SunEntity))
        m_SavedSunRotation = transform->rotation;
}


void DuskSwarmScript::RestoreSky()
{
    if (m_RestoreSun)
    {
        if (DirectionalLight* light = FindComponent<DirectionalLight>(m_SunEntity))
        {
            light->color = m_SavedSunColor;
            light->illuminance = m_SavedSunIlluminance;
        }

        if (Transform* transform = FindComponent<Transform>(m_SunEntity))
        {
            transform->rotation = m_SavedSunRotation;
            transform->Compose();
        }
    }

    m_SunEntity = Entity::Null;
    m_RestoreSun = false;
}


float DuskSwarmScript::GetSkyAngle() const
{
    const float time = m_RunTime + ( m_State == STATE_VICTORY ? m_PlayerDeathTimer : 0.0f );

    for (size_t index = 1; index < cSkyKeys.size(); index++)
    {
        const SkyKey& from = cSkyKeys[index - 1];
        const SkyKey& to = cSkyKeys[index];

        if (time < to.mTime)
            return glm::mix(from.mAngle, to.mAngle, glm::clamp(( time - from.mTime ) / ( to.mTime - from.mTime ), 0.0f, 1.0f));
    }

    return cSkyKeys.back().mAngle;
}


void DuskSwarmScript::UpdateSky()
{
    DirectionalLight* light = FindComponent<DirectionalLight>(m_SunEntity);

    if (light == nullptr)
        return;

    const float angle = GetSkyAngle();
    const Vec3 sun = Vec3(glm::cos(angle), glm::sin(angle) * glm::cos(cSkyInclination), glm::sin(angle) * glm::sin(cSkyInclination));
    const bool day = sun.y >= 0.0f;
    const Vec3 position = day ? sun : -sun;
    const float height = glm::smoothstep(0.0f, cSkyFadeHeight, position.y);
    const Vec3 color = day ? glm::mix(cSunsetColor, cSunColor, glm::smoothstep(0.0f, 0.6f, sun.y)) : cMoonColor;
    const Vec3 direction = -position;

    light->color = Vec4(color * glm::mix(cHorizonSkyBrightness, 1.0f, height), light->color.a);
    light->illuminance = ( day ? cSunIlluminance : cMoonIlluminance ) * glm::mix(cHorizonIlluminance, 1.0f, height);
    light->direction = Vec4(direction, 0.0f);

    if (Transform* transform = FindComponent<Transform>(m_SunEntity))
    {
        transform->rotation = glm::rotation(glm::normalize(DirectionalLight::cDefaultDirection), direction);
        transform->Compose();
    }
}

} // RK
