#include "pch.h"
#include "VKApp.h"

int main(int argc, char** argv) {
    auto app = new Raekor::VK::PathTracer();

    app->Run();

    delete app;

    return 0;
}