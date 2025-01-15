#define RAEKOR_SCRIPT
#include "../Engine/raekor.h"
using namespace RK;

class LightsScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(LightsScript);

    void OnBind() override
    {
        for (const auto& [entity, light] : m_Scene->Each<DirectionalLight>())
            light.color.a = 0.0f;

        BBox3D scene_bounds;

        for (const auto& [entity, transform, mesh] : m_Scene->Each<Transform, Mesh>())
            scene_bounds.Combine(mesh.bbox.Transformed(transform.worldTransform));

        constexpr float cPointLightRadius = 2.5f;
        constexpr float cPointLightIntensity = 6.0f;

        int point_light_count = 0;

        for (float x = scene_bounds.GetMin().x; x < scene_bounds.GetMax().x; x += cPointLightRadius)
        {
            for (float y = scene_bounds.GetMin().y; y < scene_bounds.GetMax().y; y += cPointLightRadius)
            {
                for (float z = scene_bounds.GetMin().z; z < scene_bounds.GetMax().z; z += cPointLightRadius)
                {
                    const Entity entity = m_Scene->CreateSpatialEntity(std::format("PointLight {}", point_light_count));

                    GetScene()->ParentTo(entity, m_Entity);

                    Transform& transform = m_Scene->Get<Transform>(entity);
                    transform.position = Vec3(x, y, z) + Vec3(cPointLightRadius) * 0.5f;
                    transform.Compose();

                    Light& light = m_Scene->Add<Light>(entity);
                    light.type = LIGHT_TYPE_POINT;
                    light.attributes.x = cPointLightRadius;

                    light.color.r = gRandomFloatZO();
                    light.color.g = gRandomFloatZO();
                    light.color.b = gRandomFloatZO();
                    light.color.a = cPointLightIntensity;

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