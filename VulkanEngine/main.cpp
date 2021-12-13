#include "Application.h"
#include <memory>

int main()
{
	auto app = std::make_unique<Application>();

	try {
		app->Run();
	}
	catch (const std::exception& e) {
		Logger::Log(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}