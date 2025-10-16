add_library(window STATIC
        window.cpp
        window.h
)

target_include_directories(window PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(window PRIVATE ${CMAKE_SOURCE_DIR}/sirius)
target_include_directories(window PRIVATE ${CMAKE_SOURCE_DIR}/sirius/third-party/imgui)

target_link_libraries(window PRIVATE vma)
