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
            std::this_thread::sleep_for(std::chrono::nanoseconds(20));
            dt = static_cast<float>(timer.stop());
        }

        delete app;
    }

    return 0;
}