#define RAEKOR_SCRIPT
#include "../Engine/raekor.h"
using namespace RK;


class AnimateSunScript : public INativeScript
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(AnimateSunScript);

    void OnUpdate(float inDeltaTime) override
    {
        Transform& transform = GetComponent<Transform>();
        Vec3 degrees = glm::degrees(glm::eulerAngles(transform.rotation));

        degrees.x += sin(m_Speed * inDeltaTime);

        if (degrees.x > 90)
            degrees.x = -90;

        if (degrees.x < -90)
            degrees.x = 90;

        transform.rotation = glm::quat(glm::radians(degrees));

        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent)
    {
    }

private:
    float m_Speed = 0.150f;
};


RTTI_DEFINE_TYPE(AnimateSunScript)
{
    RTTI_DEFINE_TYPE_INHERITANCE(AnimateSunScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(AnimateSunScript, SERIALIZE_ALL, float, "Speed", m_Speed);
}