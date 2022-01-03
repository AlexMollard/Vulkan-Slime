cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

set(CMAKE_CXX_STANDARD 20)

# Get SDL2
IF (WIN32)
    # Setting dirs for SDL2
    set(SDL2_INCLUDE_DIR C:/libs/SDL2/include)
    set(SDL2_LIB_DIR C:/libs/SDL2/lib/x64)

    include_directories(${SDL2_INCLUDE_DIR})
    link_directories(${SDL2_LIB_DIR})
ELSE()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
ENDIF()

#Include ProjectFolders
include_directories(Src Vulkan)

#Get VkBootstrap
include(FetchContent)
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

# Including SDL2 dirs
include_directories(${SDL2_INCLUDE_DIR})
link_directories(${SDL2_LIB_DIR})

# Set code directories
FILE(GLOB FILES_TO_BUILD Src/*.h Src/*.cpp Vulkan/*.h Vulkan/*.cpp)

# Compiling project
add_executable(VulkanSlime ${FILES_TO_BUILD} Vulkan/VulkanInitializers.cpp Vulkan/VulkanInitializers.h)

# Finding and including Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

# Linking SDL2
IF (WIN32)
target_link_libraries(VulkanSlime SDL2main SDL2)
ELSE()
target_link_libraries(VulkanSlime ${SDL2_LIBRARIES})
ENDIF()

#Linking VkBootstrap
target_link_libraries(VulkanSlime vk-bootstrap::vk-bootstrap)
