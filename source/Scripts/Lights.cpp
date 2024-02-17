#define RAEKOR_SCRIPT
#include "Raekor/raekor.h"
using namespace Raekor;

class LightsScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(LightsScript);

    void OnBind() override
    {
        for (const auto& [entity, light] : m_Scene->Each<DirectionalLight>())
            light.colour.a = 0.0f;

        BBox3D scene_bounds;

        for (const auto& [entity, transform, mesh] : m_Scene->Each<Transform, Mesh>())
            scene_bounds.Combine(BBox3D(mesh.aabb[0], mesh.aabb[1]).Transform(transform.worldTransform));

        constexpr auto cPointLightRadius = 2.5f;
        constexpr auto cPointLightIntensity = 6.0f;

        auto point_light_count = 0u;

        for (auto x = scene_bounds.GetMin().x; x < scene_bounds.GetMax().x; x += cPointLightRadius)
        {
            for (auto y = scene_bounds.GetMin().y; y < scene_bounds.GetMax().y; y += cPointLightRadius)
            {
                for (auto z = scene_bounds.GetMin().z; z < scene_bounds.GetMax().z; z += cPointLightRadius)
                {
                    const auto entity = m_Scene->CreateSpatialEntity(std::format("PointLight {}", point_light_count));

                    NodeSystem::sAppend(*m_Scene, m_Entity, m_Scene->Get<Node>(m_Entity), entity, m_Scene->Get<Node>(entity));

                    auto& transform = m_Scene->Get<Transform>(entity);
                    transform.position = Vec3(x, y, z) + Vec3(cPointLightRadius) * 0.5f;
                    transform.Compose();

                    auto& light = m_Scene->Add<Light>(entity);
                    light.type = LIGHT_TYPE_POINT;
                    light.attributes.x = cPointLightRadius;

                    light.colour.r = gRandomFloatZO();
                    light.colour.g = gRandomFloatZO();
                    light.colour.b = gRandomFloatZO();
                    light.colour.a = cPointLightIntensity;

                    point_light_count++;
                }
            }
        }

        GetComponent<Name>().name = std::format("PointLights ({})", point_light_count);
    }

    void OnUpdate(float inDeltaTime) override
    {
        Transform& transform = GetComponent<Transform>();

        transform.rotation *= glm::angleAxis(float(M_PI) / 128.0f, Vec3(0.0f, 1.0f, 0.0f));

        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent) {}
};


RTTI_DEFINE_TYPE(LightsScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(LightsScript, INativeScript);
}