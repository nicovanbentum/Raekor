#include "pch.h"
#include "editor.h"
#include "Raekor/ecs.h"

using namespace Raekor;

int main(int argc, char** argv) {
    auto app =  new Editor();

    app->Run();

    delete app;

    return 0;
}