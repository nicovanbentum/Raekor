#include "pch.h"
#include "apps.h"
#include "timer.h"
#include "camera.h"
#include "editor.h"
#include "renderer.h"
#include "VK/VKApp.h"

int main(int argc, char** argv) {
    Raekor::Application* app;

    if (argc) {
        std::cout << argv[1] << '\n';
    }

    if (argc > 1 && strcmp(argv[1], "-gl") == 0) {
        PATHTRACE = false;
        app = new Raekor::Editor();
    }
    else if (argc > 1 && strcmp(argv[1], "-vk") == 0) {
        PATHTRACE = true;
        app = new Raekor::VK::PathTracer();
    }
    
    app->run();

    delete app;

    return 0;
}