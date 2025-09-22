add_executable(main
        main.cpp
        ${CMAKE_SOURCE_DIR}/sirius.rc
)

add_dependencies(main shaders)

target_link_libraries(main PRIVATE
        game
        engine
        fmt::fmt
)

target_include_directories(main PRIVATE
        ${CMAKE_SOURCE_DIR}/sirius
        ${CMAKE_SOURCE_DIR}
)