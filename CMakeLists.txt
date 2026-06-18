cmake_minimum_required(VERSION 3.24)

project(MappyZ
    VERSION 0.1.0
    DESCRIPTION "Modern gamepad remapping runtime"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MappyZ
    source/App/Main.cpp
)

target_compile_features(MappyZ PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MappyZ PRIVATE /W4)
else()
    target_compile_options(MappyZ PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

