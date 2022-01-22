cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

# ----------------------------------------------------------
# VkBootstrap Bootstarp lib for vulkan
FetchContent_Declare(
        vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG
)
FetchContent_GetProperties(vk_bootstrap)
if(NOT vk_bootstrap_POPULATED)
    message("Cloning vk_bootstrap")
    FetchContent_Populate(vk_bootstrap)
    add_subdirectory(
            ${vk_bootstrap_SOURCE_DIR}
            ${vk_bootstrap_BINARY_DIR})
endif()

# ----------------------------------------------------------
# VulkanMemoryAllocator A wrapper for some vulkan memory functions
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG
)
FetchContent_GetProperties(VulkanMemoryAllocator)
if(NOT vulkanmemoryallocator_POPULATED)
    message("Cloning VulkanMemoryAllocator")
    FetchContent_Populate(vulkanmemoryallocator)
    add_subdirectory(
            ${vulkanmemoryallocator_SOURCE_DIR}
            ${vulkanmemoryallocator_BINARY_DIR})
endif()

# ----------------------------------------------------------
# entt A entity component system for c++
FetchContent_Declare(
        Entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG
)
FetchContent_GetProperties(Entt)
if(NOT entt_POPULATED)
    message("Cloning entt")
    FetchContent_Populate(entt)
    add_subdirectory(
            ${entt_SOURCE_DIR}
            ${entt_BINARY_DIR})
endif()

# ----------------------------------------------------------
# spdlog A fast and simple to use logging library.
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG       )

FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
    message("Cloning spdlod")
    FetchContent_Populate(spdlog)
    add_subdirectory(
            ${spdlog_SOURCE_DIR}
            ${spdlog_BINARY_DIR})
endif()

# ----------------------------------------------------------
# glm big math lib for graphics math
FetchContent_Declare(
        GLM
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        )

FetchContent_GetProperties(GLM)
if(NOT glm_POPULATED)
    message("Cloning GLM")
    FetchContent_Populate(GLM)
    add_subdirectory(
            ${glm_SOURCE_DIR}
            ${glm_BINARY_DIR})
endif()

# ----------------------------------------------------------
# Tiny_OBJ_Loader Model loader
FetchContent_Declare(
        Tiny_OBJ_Loader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG
)

FetchContent_GetProperties(Tiny_OBJ_Loader)
if(NOT tiny_obj_loader_POPULATED)
    message("Cloning Tiny_OBJ_Loader")
    FetchContent_Populate(Tiny_OBJ_Loader)
    add_subdirectory(
            ${tiny_obj_loader_SOURCE_DIR}
            ${tiny_obj_loader_BINARY_DIR})
endif()

# ----------------------------------------------------------
# STB Image light weight image loader
FetchContent_Declare(
        STB_Image
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG
)

FetchContent_GetProperties(STB_Image)
if(NOT stb_image_POPULATED)
    message("Cloning STB_Image")
    FetchContent_Populate(STB_Image)
endif()

# ----------------------------------------------------------
# ImGui Bloat-free Graphical User interface
FetchContent_Declare(
        ImGui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG
)
FetchContent_GetProperties(ImGui)
if(NOT imgui_POPULATED)
    message("Cloning ImGui")
    FetchContent_Populate(ImGui)
endif()

# ----------------------------------------------------------
# ImGuizmo gizmos for ImGui
FetchContent_Declare(
        ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG
)
FetchContent_GetProperties(ImGuizmo)
if(NOT imguizmo_POPULATED)
    message("Cloning ImGuizmo")
    FetchContent_Populate(ImGuizmo)
endif()

# ----------------------------------------------------------
# SDL2 Window/Surface creator and handler
FetchContent_Declare(
        SDL
        GIT_REPOSITORY https://github.com/davidsiaw/SDL2.git
        GIT_TAG
)

FetchContent_GetProperties(SDL)
if(NOT sdl_POPULATED)
    message("Cloning SDL")
    FetchContent_Populate(SDL)
    add_subdirectory(
            ${sdl_SOURCE_DIR}
            ${sdl_BINARY_DIR})
endif()