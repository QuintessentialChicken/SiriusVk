add_library(window STATIC
        window.cpp
        window.h
)

target_include_directories(window PRIVATE ${CMAKE_SOURCE_DIR})