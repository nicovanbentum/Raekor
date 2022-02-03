#include "pch.h"
#include "editor.h"

int main(int argc, char** argv) {
    auto app =  new Raekor::Editor();

    app->run();

    delete app;

    return 0;
}