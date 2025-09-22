cmake_minimum_required(VERSION 4.0.2)
project(SiriusVk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
include(FetchContent)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
)
FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG master
)

FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(vma)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)

include_directories("$ENV{VULKAN_SDK}/Include")
include_directories("$ENV{VULKAN_SDK}/Include/glm")

add_subdirectory(game)
add_subdirectory(main)
add_subdirectory(sirius)
