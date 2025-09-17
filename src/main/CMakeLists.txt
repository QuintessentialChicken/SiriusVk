add_executable(main main.cpp)

target_link_libraries(main PRIVATE
        game
        engine
)

target_include_directories(main PRIVATE
        ${CMAKE_SOURCE_DIR}/sirius
        ${CMAKE_SOURCE_DIR}
)