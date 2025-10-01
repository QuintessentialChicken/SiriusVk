add_library(engine INTERFACE)

target_link_libraries(engine INTERFACE
        core
        graphics
        window
        third-party
        Vulkan::Vulkan
)

add_subdirectory(core)
add_subdirectory(graphics)
add_subdirectory(window)
add_subdirectory(shaders)
add_subdirectory(third-party)
