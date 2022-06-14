#include "pch.h"
#include "VKApp.h"

int main(int argc, char** argv) {
    auto app = Raekor::VK::PathTracer();

    app.Run();

    return 0;
}