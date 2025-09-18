find_package(Vulkan REQUIRED)
include_directories("$ENV{VULKAN_SDK}/Include")
include_directories("$ENV{VULKAN_SDK}/Include/glm")

add_library(graphics STATIC
        device.cpp
        device.h
        pipeline.cpp
        pipeline.h
        swapchain.cpp
        swapchain.h
        renderer.cpp
        renderer.h
        model.cpp
        model.h
)

target_include_directories(graphics PRIVATE ${CMAKE_SOURCE_DIR}/sirius)

target_link_libraries(graphics PRIVATE Vulkan::Vulkan)