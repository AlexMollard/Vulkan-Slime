#include "../VulkanEngine/Application.h"
#include <memory>

int main (int argc, char *argv[])
{
    Application app{};

    try {
        app.Run();
    }
    catch (const std::exception& e) {
        Logger::Log(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}