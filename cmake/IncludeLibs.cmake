cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

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
IF (WIN32)
    target_link_libraries(VulkanSlime SDL2main SDL2-static)
    include_directories(${sdl_SOURCE_DIR}/include)
ELSE ()
    target_link_libraries(VulkanSlime ${SDL2_LIBRARIES})
ENDIF ()

#include ImGuizmo
include_directories(${imguizmo_SOURCE_DIR})

#include ImGuizmo
include_directories(${spirv_ref_SOURCE_DIR})

#include Tracy
include_directories(${tracy_SOURCE_DIR})

#include fmt
target_link_libraries(VulkanSlime fmt::fmt)

#include lz4
include_directories(${lz4_SOURCE_DIR}/lib)
target_link_libraries(VulkanSlime lz4_static)

#include assimp
include_directories(${assimp_SOURCE_DIR}/include)
target_link_libraries(VulkanSlime assimp)

#include nvtt
include_directories(${nvtt_SOURCE_DIR}/src)
target_link_libraries(VulkanSlime nvtt)

#include nvtt
include_directories(${tinygltf_SOURCE_DIR})
target_link_libraries(VulkanSlime tinygltf)

#Compiling and Linking ImGui
target_sources(VulkanSlime PRIVATE
        "${imgui_SOURCE_DIR}/imgui.h"
        "${imgui_SOURCE_DIR}/imgui.cpp"

        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"

        "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp"

        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
        )
# Compile ImGui as static library
file(GLOB IMGUI_FILES ${imgui_SOURCE_DIR}/*.cpp)
add_library("ImGui" STATIC ${IMGUI_FILES})
target_include_directories("ImGui" PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_link_libraries(VulkanSlime ImGui)
