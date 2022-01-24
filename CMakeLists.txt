cmake_minimum_required(VERSION 3.21)
project(VulkanSlime)

set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

#disable in lining for profiler
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

#enable multithreading for linux
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

#Fetch all external libs
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(${CMAKE_MODULE_PATH}/FetchLibs.cmake)

#Include ProjectFolders
include_directories(Src Vulkan Assetlib)

# Set code directories
FILE(GLOB FILES_TO_BUILD
        Src/*.h Src/*.cpp Src/json.hpp
        Vulkan/*.h Vulkan/*.cpp
        Assetlib/*.h Assetlib/*.cpp
        ${spirv_ref_SOURCE_DIR}/*.h ${spirv_ref_SOURCE_DIR}/*.c
        )

# Compiling project
add_executable(VulkanSlime ${FILES_TO_BUILD})

#define debug
target_compile_definitions(VulkanSlime PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

# Finding and including Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

#Include all external libs
include(${CMAKE_MODULE_PATH}/IncludeLibs.cmake)

#Compile Shaders
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
        )

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
        SOURCES ${GLSL_SOURCE_FILES}
)
