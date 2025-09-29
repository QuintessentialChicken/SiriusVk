find_package(Vulkan REQUIRED)
include_directories("$ENV{VULKAN_SDK}/Include")
include_directories("$ENV{VULKAN_SDK}/Include/glm")

add_library(graphics STATIC
        vkRenderer.cpp
        vkRenderer.h
        renderer.cpp
        renderer.h
        descriptors.cpp
        descriptors.h
        pipelines.cpp
        pipelines.h
        initializers.cpp
        initializers.h
)

target_include_directories(graphics PRIVATE ${CMAKE_SOURCE_DIR}/sirius)

target_link_libraries(graphics PUBLIC Vulkan::Vulkan fmt::fmt vma)