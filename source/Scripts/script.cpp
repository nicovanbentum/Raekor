#include "Raekor/raekor.h"
using namespace Raekor;

class TestScript : public INativeScript {
public:
    void OnUpdate(float inDeltaTime) override
    {
        GetComponent<Name>().name = "I ROTATE";
        GetComponent<Name>().name = "POTATO ROTATO";

        //transform.rotation.y = 33.14f;
        auto& transform = GetComponent<Transform>();
        GetComponent<Transform>().position.y += glm::sin(time) * inDeltaTime;
        GetComponent<Transform>().Compose();

        if (GetInput()->sIsKeyPressed(SDL_SCANCODE_W))
            transform.position.x += 1.0f * inDeltaTime;

        /*auto& aabb = GetComponent<Mesh>().aabb;

        aabb[1] += 10.0f * inDeltaTime;*/

        time += inDeltaTime;
    }

    void OnEvent(const SDL_Event& inEvent)
    {

    }

private:
    float speed = 0.02f;
    float time = 0.0f;
};

DEFINE_SCRIPT_CLASS(TestScript);