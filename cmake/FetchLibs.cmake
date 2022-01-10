cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

#Get VkBootstrap
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

#Get VulkanMemoryAllocator
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

#Get GLM
FetchContent_Declare(
        GLM
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG
)
FetchContent_MakeAvailable(GLM)

#Get Tiny_OBJ_Loader
FetchContent_Declare(
        Tiny_OBJ_Loader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG
)
FetchContent_MakeAvailable(Tiny_OBJ_Loader)

# Get SDL2
IF (WIN32)
    # Setting dirs for SDL2
    set(SDL2_INCLUDE_DIR C:/libs/SDL2/include)
    set(SDL2_LIB_DIR C:/libs/SDL2/lib/x64)

    include_directories(${SDL2_INCLUDE_DIR})
    link_directories(${SDL2_LIB_DIR})
ELSE ()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    link_directories(${SDL2_LIB_DIR})
ENDIF ()

#Get ImGui
#FetchContent_Declare(
#        ImGui
#        GIT_REPOSITORY https://github.com/ocornut/imgui.git
#        GIT_TAG
#)
#FetchContent_MakeAvailable(ImGui)
