#include "pch.h"
#include "apps.h"
#include "timer.h"
#include "camera.h"
#include "editor.h"
#include "renderer.h"

int main(int argc, char** argv) {
    {
        Raekor::WindowApplication* app = new Raekor::Editor();

        float dt = 0;
        Raekor::Timer timer;

        while (app->running) {
            timer.start();
            app->update(dt);
            dt = static_cast<float>(timer.stop());
        }

        delete app;
    }

    return 0;
}