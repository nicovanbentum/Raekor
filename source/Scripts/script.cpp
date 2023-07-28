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
        transform.scale = Vec3(1, 2, 3);
        GetComponent<Transform>().rotation.x += 3.14159f * inDeltaTime;
        GetComponent<Transform>().rotation.y += 32.0f * inDeltaTime;
        GetComponent<Transform>().Compose();

        GetComponent<Transform>().Print();
        std::cout << GetComponent<Transform>().rotation.y << '\n';

        auto& aabb = GetComponent<Mesh>().aabb;

        aabb[1] += 10.0f * inDeltaTime;


    }

private:
    float speed = 0.02f;
};

RAEKOR_SCRIPT_CLASS(TestScript);