add_library(game
        app.cpp
        app.h
)

target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/sirius)