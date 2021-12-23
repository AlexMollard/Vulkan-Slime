cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

set(CMAKE_CXX_STANDARD 20)

# Setting dirs for SDL2
set(SDL2_INCLUDE_DIR C:/libs/SDL2/include)
set(SDL2_LIB_DIR C:/libs/SDL2/lib/x64)

# Including SDL2 dirs
include_directories(${SDL2_INCLUDE_DIR})
link_directories(${SDL2_LIB_DIR})

# Set code directories
FILE(GLOB FILES_TO_BUILD Src/*.h Src/*.cpp Vulkan/*.h Vulkan/*.cpp)

# Compiling project
add_executable(VulkanSlime ${FILES_TO_BUILD})

# Finding and including Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

# Linking SDL2
target_link_libraries(${PROJECT_NAME} SDL2main SDL2)