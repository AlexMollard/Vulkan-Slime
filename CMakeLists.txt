# Cmake minimum Version to compile
cmake_minimum_required(VERSION 3.21)

# Project name to help with later code
project(VulkanTest)

# include stuff needed for ExternalProject_Add
include(ExternalProject)

# Set the external path for third party libs
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/External)

# Clone GLFW main branch from GitHub
ExternalProject_Add(glfw3 GIT_REPOSITORY https://github.com/glfw/glfw CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION})

# Clone GLM main branch from GitHub
ExternalProject_Add(glm GIT_REPOSITORY https://github.com/g-truc/glm CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION})

# Clone GLM main branch from GitHub
ExternalProject_Add(tiny-obj GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION})

# Include the compiled files from the third party libs
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

# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

# GLFW vars to lower compile time
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Include GLFW
add_dependencies(VulkanTest glfw3)
target_link_libraries(VulkanTest ${EXTERNAL_INSTALL_LOCATION}/lib/glfw3.lib)

# GLM
add_dependencies(VulkanTest glm)

# TinyOBJ
add_dependencies(VulkanTest tiny-obj)
target_link_libraries(VulkanTest ${EXTERNAL_INSTALL_LOCATION}/lib/tinyobjloader.lib)

# STB
INCLUDE_DIRECTORIES(Dependencies/stb)