cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

#disable in lining for profiler
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

#Fetch all external libs
include(${CMAKE_MODULE_PATH}/FetchLibs.cmake)

#Include ProjectFolders
include_directories(Src Vulkan)

# Set code directories
FILE(GLOB FILES_TO_BUILD Src/*.h Src/*.cpp Vulkan/*.h Vulkan/*.cpp)

# Compiling project
add_executable(VulkanSlime ${FILES_TO_BUILD})

# Finding and including Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

#Include all external libs
include(${CMAKE_MODULE_PATH}/IncludeLibs.cmake)
