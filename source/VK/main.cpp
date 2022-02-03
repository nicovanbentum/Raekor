#include "pch.h"
#include "VKApp.h"

int main(int argc, char** argv) {
    auto app = new Raekor::VK::PathTracer();

    app->run();

    delete app;

    return 0;
}