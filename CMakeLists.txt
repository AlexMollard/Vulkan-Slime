cmake_minimum_required(VERSION 3.21)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE VulkanEngineSources VulkanEngine/*.cpp VulkanEngine/*.h)

add_executable(VulkanTest Project/main.cpp ${VulkanEngineSources})

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

INCLUDE_DIRECTORIES(lib/glm)

INCLUDE_DIRECTORIES(lib/stb)