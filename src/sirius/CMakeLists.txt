add_library(engine INTERFACE)

target_link_libraries(engine INTERFACE
        core
        graphics
        window
)

add_subdirectory(core)
add_subdirectory(graphics)
add_subdirectory(window)