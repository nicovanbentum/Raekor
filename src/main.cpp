#include "pch.h"
#include "editor.h"
#include "VK/VKApp.h"

int main(int argc, char** argv) {
    Raekor::Application* app;

    if (argc > 1 && strcmp(argv[1], "-vk") == 0) {
        PATHTRACE = true;
        app = new Raekor::VK::PathTracer();
    }
    else {
        PATHTRACE = false;
        app =  new Raekor::Editor();
    }

    app->run();

    delete app;

    return 0;
}