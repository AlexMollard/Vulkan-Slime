#include "../VulkanEngine/application.h"

int main() {
    application app{};

    try {
        app.run();
    }
    catch (const cppError &e) {
        logger::log(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}