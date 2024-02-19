#define RAEKOR_SCRIPT
#include "Raekor/raekor.h"
using namespace Raekor;


class TestScript : public INativeScript 
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(TestScript);

    void OnUpdate(float inDeltaTime) override
    {
        if (!m_Enabled)
            return;

        auto& transform = GetComponent<Transform>();

        if (m_Input->IsKeyPressed(SDL_SCANCODE_W))
            transform.position.x += m_Speed * inDeltaTime;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_S))
            transform.position.x -= m_Speed * inDeltaTime;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_A))
            transform.position.z += m_Speed * inDeltaTime;

        if (m_Input->IsKeyPressed(SDL_SCANCODE_D))
            transform.position.z -= m_Speed * inDeltaTime;

        transform.Compose();
    }

    void OnEvent(const SDL_Event& inEvent) 
    {
    }

private:
    int m_Value = 12;
    float m_Speed = 1.0f;
    bool m_Enabled = true;
};


RTTI_DEFINE_TYPE(TestScript) 
{
    RTTI_DEFINE_TYPE_INHERITANCE(TestScript, INativeScript);

    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF(bool),  "Enabled", m_Enabled);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF(int),   "Value",   m_Value);
    RTTI_DEFINE_SCRIPT_MEMBER(TestScript, SERIALIZE_ALL, &RTTI_OF(float), "Speed",   m_Speed);
}
