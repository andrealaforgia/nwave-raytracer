cmake_minimum_required(VERSION 3.16)
project(nwave-raytracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(DOUBLE_PRECISION ON CACHE BOOL "Use double precision math" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest yaml-cpp JoltPhysics)

option(NWAVE_ENABLE_GPU "Enable GPU compute rendering (requires Metal on macOS)" OFF)

if(NWAVE_ENABLE_GPU)
    if(APPLE)
        find_library(METAL_FRAMEWORK Metal)
        find_library(FOUNDATION_FRAMEWORK Foundation)
        if(METAL_FRAMEWORK AND FOUNDATION_FRAMEWORK)
            enable_language(OBJCXX)
            message(STATUS "Metal framework found: ${METAL_FRAMEWORK}")
            message(STATUS "Foundation framework found: ${FOUNDATION_FRAMEWORK}")
            set(NWAVE_HAS_METAL ON)
        else()
            message(WARNING "NWAVE_ENABLE_GPU=ON but Metal/Foundation frameworks not found")
        endif()
    else()
        message(WARNING "NWAVE_ENABLE_GPU=ON but Metal is only available on macOS; GPU support disabled")
    endif()
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
