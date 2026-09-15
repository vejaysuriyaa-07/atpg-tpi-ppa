cmake_minimum_required(VERSION 3.10)
project(bist-simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2)

add_executable(bist-simulator
    src/main.cpp
    src/Circuit.cpp
    src/LFSR.cpp
    src/MISR.cpp
    src/BISTController.cpp
)

target_include_directories(bist-simulator PRIVATE include)
