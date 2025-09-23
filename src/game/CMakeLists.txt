add_library(game
        app.cpp
        app.h
)

target_link_libraries(game fmt::fmt)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/sirius)
target_include_directories(game PRIVATE ${vma_SOURCE_DIR}/include)
