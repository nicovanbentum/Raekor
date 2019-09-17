#include "pch.h"
#include "app.h"

int main(int argc, char** argv) {
    
    auto app = Raekor::Application();
    app.vulkan_main();
    system("PAUSE");
    return 0;
}