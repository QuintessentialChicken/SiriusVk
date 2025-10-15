find_package(Vulkan REQUIRED)

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
        asset_loader.cpp
        asset_loader.h
)

target_include_directories(graphics PRIVATE ${CMAKE_SOURCE_DIR}/sirius)
target_include_directories(graphics PRIVATE "$ENV{VULKAN_SDK}/Include")
target_include_directories(graphics PRIVATE "$ENV{VULKAN_SDK}/Include/glm")
target_include_directories(graphics PRIVATE ${CMAKE_SOURCE_DIR}/sirius/third-party/imgui)
target_link_libraries(graphics PUBLIC Vulkan::Vulkan fmt::fmt vma fastgltf::fastgltf)