# Cmake minimum Version to compile
cmake_minimum_required(VERSION 3.21)

# Project name to help with later code
project(VulkanTest)

include(ExternalProject)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/External)

ExternalProject_Add(glfw3
        GIT_REPOSITORY https://github.com/glfw/glfw
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        )

include_directories(${EXTERNAL_INSTALL_LOCATION}/include)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

# CMAKE version
set(CMAKE_CXX_STANDARD 23)

# Main Files
file(GLOB_RECURSE ProjectSources Project/*.cpp Project/*.h)

# Vulkan Files
file(GLOB_RECURSE VulkanEngineSources VulkanEngine/*.cpp VulkanEngine/*.h)

# Files to compile
add_executable(VulkanTest ${ProjectSources} ${VulkanEngineSources})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_dependencies(VulkanTest glfw3)
target_link_libraries(VulkanTest ${EXTERNAL_INSTALL_LOCATION}/lib/glfw3.lib)

# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)


# Include GLFW
#add_subdirectory(Dependencies/glfw)
#target_link_libraries(${PROJECT_NAME} glfw)

# GLM
INCLUDE_DIRECTORIES(Dependencies/glm)

# TinyOBJ
INCLUDE_DIRECTORIES(Dependencies/tinyobj)

# STB
INCLUDE_DIRECTORIES(Dependencies/stb)