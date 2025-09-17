find_package(Vulkan REQUIRED)
include_directories("$ENV{VULKAN_SDK}/Include")
include_directories("$ENV{VULKAN_SDK}/Include/glm")

add_library(window STATIC
        window.cpp
        window.h
)

target_include_directories(window PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(window PRIVATE Vulkan::Vulkan)