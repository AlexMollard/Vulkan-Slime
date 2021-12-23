#include <iostream>
#include "../Vulkan/VulkanEngine.h"

int main(int argc, char* args[]) {
    VulkanEngine engine;

    engine.init();
    engine.run();
    engine.cleanup();

    return 0;
}
