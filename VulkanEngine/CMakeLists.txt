FILE(GLOB sources ${CMAKE_SOURCE_DIR}/*.h ${CMAKE_SOURCE_DIR}/*.cpp)
add_library(VulkanEngine ${sources})

target_link_libraries(${PROJECT_NAME} VulkanEngine)

if(WIN32)
    target_include_directories (VulkanEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
else(WIN32)
    target_include_directories (VulkanEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif(WIN32)