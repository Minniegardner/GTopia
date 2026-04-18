cmake_minimum_required(VERSION 3.16)

project(GrowClientWin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(GrowClientWin WIN32
    src/main.cpp
)

target_compile_definitions(GrowClientWin PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(GrowClientWin PRIVATE /W4 /permissive-)
else()
    target_compile_options(GrowClientWin PRIVATE -Wall -Wextra -Wpedantic)
endif()
