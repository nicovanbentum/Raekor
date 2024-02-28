#define RAEKOR_SCRIPT
#include "raekor.h"
using namespace Raekor;

class TestScript : public INativeScript {
public:
    void OnUpdate(float inDeltaTime) override 
    {
        // GetComponent<Name>().name = "I ROTATE";
        // GetComponent<Name>().name = "POTATO ROTATO";

        // auto& transform = GetComponent<Transform>();
        // transform.rotation.y = 33.14f;
        // transform.compose();

        // transform.print();
        // std::cout << transform.rotation.y << '\n';

        // //transform.rotation.x += 3.14159f / 10000.0f * dt;
    }

private:
    float speed = 0.02f;
};