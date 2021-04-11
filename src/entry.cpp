#include "pch.h"
#include "apps.h"
#include "timer.h"
#include "camera.h"
#include "editor.h"
#include "renderer.h"

int main(int argc, char** argv) {
    {
        auto app = Raekor::Editor();

        float dt = 0;
        Raekor::Timer timer;

        while (app.running) {
            timer.start();
            app.update(dt);
            dt = static_cast<float>(timer.stop());
        }
    }

    return 0;
}