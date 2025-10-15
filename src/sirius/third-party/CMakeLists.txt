add_library(third-party
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_impl_win32.h
        imgui/imgui_impl_win32.cpp
        imgui/imgui_impl_vulkan.h
        imgui/imgui_impl_vulkan.cpp
        imgui/imgui_demo.cpp
)

add_subdirectory(fastgltf)