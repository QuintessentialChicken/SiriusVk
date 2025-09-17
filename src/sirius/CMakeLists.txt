add_library(engine INTERFACE)

target_link_libraries(engine INTERFACE
        core
)

add_subdirectory(core)