#include "pch.h"
#include "app.h"

int main(int argc, char** argv) {
    
    auto app = Raekor::Application();
    for (;;) {
        app.run();
    }

    return 0;
}