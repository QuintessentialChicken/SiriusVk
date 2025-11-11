add_library(engine INTERFACE)
find_package(Vulkan REQUIRED)

target_link_libraries(engine INTERFACE
        core
        graphics
        input
        window
        third-party
        Vulkan::Vulkan
)

target_include_directories(engine INTERFACE ${CMAKE_SOURCE_DIR}/sirius)

add_subdirectory(core)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(window)
add_subdirectory(shaders)
add_subdirectory(third-party)
