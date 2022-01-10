cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

#include TinyOBJ
include_directories(${tiny_obj_loader_SOURCE_DIR})

# Linking SDL2
IF (WIN32)
    target_link_libraries(VulkanSlime SDL2main SDL2)
ELSE ()
    target_link_libraries(VulkanSlime ${SDL2_LIBRARIES})
ENDIF ()

#Linking VkBootstrap
target_link_libraries(VulkanSlime vk-bootstrap::vk-bootstrap)

#linking VulkanMemoryAllocator
target_link_libraries(VulkanSlime VulkanMemoryAllocator)

#Linking GLM
target_link_libraries(VulkanSlime glm::glm)

#Linking ImGui
#target_sources(VulkanSlime PRIVATE
#        "${imgui_SOURCE_DIR}/imgui.h"
#        "${imgui_SOURCE_DIR}/imgui.cpp"
#
#        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
#        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
#        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
#
#        "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
#        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp"
#        )
#include_directories(${imgui_SOURCE_DIR})
#include_directories(${imgui_SOURCE_DIR}/backends)
