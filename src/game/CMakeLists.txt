add_library(game
        app.cpp
        app.h
)

target_link_libraries(game PRIVATE engine)