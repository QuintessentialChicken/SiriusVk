cmake_minimum_required(VERSION 4.0.2)
project(SiriusVk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

include_directories("$ENV{VULKAN_SDK}/Include")
include_directories("$ENV{VULKAN_SDK}/Include/glm")

add_subdirectory(game)
add_subdirectory(main)
add_subdirectory(sirius)