add_library(input STATIC
        input_manager.cpp
        input_manager.h
)

target_include_directories(input PRIVATE ${CMAKE_SOURCE_DIR}/sirius)
target_link_libraries(input PRIVATE vma)

