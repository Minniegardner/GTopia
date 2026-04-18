cmake_minimum_required(VERSION 3.16)

project(ReGTByVinz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/enet ${CMAKE_CURRENT_BINARY_DIR}/enet)

add_executable(ReGTByVinz WIN32
    src/main.cpp
    src/ReGTClient.cpp
)

target_include_directories(ReGTByVinz PRIVATE
    src
    ${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/enet/include
)

target_link_libraries(ReGTByVinz PRIVATE enet)

target_compile_definitions(ReGTByVinz PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(ReGTByVinz PRIVATE /W4 /permissive-)
else()
    target_compile_options(ReGTByVinz PRIVATE -Wall -Wextra -Wpedantic)
endif()
