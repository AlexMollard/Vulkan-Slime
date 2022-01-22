cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

#include stbImage
include_directories(${stb_image_SOURCE_DIR})

#include TinyOBJ
include_directories(${tiny_obj_loader_SOURCE_DIR})

#Including Entt
include_directories(${entt_SOURCE_DIR}/src/entt)

#include spdlog
include_directories(${spdlog_SOURCE_DIR}/include)

#include GLM
include_directories(${glm_SOURCE_DIR})

#include VulkanMemoryAllocator
include_directories(${vulkanmemoryallocator_SOURCE_DIR}/include)

#include VkBootstrap
include_directories(${vk_bootstrap_SOURCE_DIR}/src)
target_link_libraries(VulkanSlime vk-bootstrap::vk-bootstrap)

# include SDL2
include_directories(${sdl_SOURCE_DIR}/include)
target_link_libraries(VulkanSlime SDL2-static)

#include ImGuizmo
include_directories(${imguizmo_SOURCE_DIR})

#Compiling and Linking ImGui
target_sources(VulkanSlime PRIVATE
        "${imgui_SOURCE_DIR}/imgui.h"
        "${imgui_SOURCE_DIR}/imgui.cpp"

        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"

        "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp"
        )
# Compile ImGui as static library
file(GLOB IMGUI_FILES ${imgui_SOURCE_DIR}/*.cpp)
add_library("ImGui" STATIC ${IMGUI_FILES})
target_include_directories("ImGui" PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_link_libraries(VulkanSlime ImGui)
