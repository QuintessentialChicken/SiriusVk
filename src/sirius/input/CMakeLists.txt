add_library(input
        input_manager.cpp
        input_manager.h
        keyboard.cpp
        keyboard.h
        mouse.cpp
        mouse.h
)

target_link_libraries(input PUBLIC graphics)
