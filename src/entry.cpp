#include "pch.h"
#include "apps.h"
#include "timer.h"
#include "camera.h"
#include "editor.h"
#include "renderer.h"
#include "VK/VKEntry.h"

int main(int argc, char** argv) {
    {
        Raekor::WindowApplication* app;

        PATHTRACE = 1;
        
        if (argc) {
            std::cout << argv[1] << '\n';
        }

        if (argc > 1 && strcmp(argv[1], "-vk") == 0) {
            PATHTRACE = 1;
        }
        else if (argc > 1 && strcmp(argv[1], "-gl") == 0) {
            PATHTRACE = 0;
        }
        
        if (PATHTRACE) {
            app = new Raekor::VulkanPathTracer();
        }
        else {
            app = new Raekor::Editor();
        }

        float dt = 0;
        Raekor::Timer timer;

        while (app->running) {
            timer.start();
            app->update(dt);
            //std::this_thread::sleep_for(std::chrono::nanoseconds(20));
            dt = static_cast<float>(timer.stop());
        }

        delete app;
    }

    return 0;
}