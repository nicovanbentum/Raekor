#include "raekor.h"
using namespace Raekor;

class TestScript : public INativeScript {
public:
    void update(float dt) override 
    {
        GetComponent<Name>().name = "I ROTATE";
        GetComponent<Name>().name = "POTATO ROTATO";

        auto& transform = GetComponent<Transform>();
        transform.rotation.y = 33.14f;
        transform.compose();

        transform.print();
        std::cout << transform.rotation.y << '\n';

        //transform.rotation.x += 3.14159f / 10000.0f * dt;
    }

private:
    float speed = 0.02f;
};

RAEKOR_SCRIPT_CLASS(TestScript);