#include "raekor.h"
using namespace Raekor;

class TestScript : public INativeScript {
public:
    void update(float dt) override 
    {
        GetComponent<Name>().name = "I ROTATE";

        auto& transform = GetComponent<Transform>();
        transform.rotation.y += 3.14159f / 10000.0f * dt;
        if(transform.rotation.y > 3.14159f) {
            transform.rotation.y = 0.0f;
        }
        //transform.rotation.x += 3.14159f / 10000.0f * dt;
    }

private:
    float speed = 0.02f;
};

RAEKOR_SCRIPT_CLASS(TestScript);