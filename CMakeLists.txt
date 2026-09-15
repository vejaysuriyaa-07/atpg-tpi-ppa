cmake_minimum_required(VERSION 3.10)
# CHANGE TO VERSION 2.8 TO RUN ON VITERBI SERVER
project(simulator)

# Optional readline support for line editing
option(USE_READLINE "Enable GNU Readline for line editing support" ON)

set(default_build_type "Debug")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-std=c++17")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
include_directories(${CMAKE_SOURCE_DIR}/includes)

set(CORE_SOURCES
    src/Command.cpp
    src/Node.cpp
    src/Simulator.cpp
    src/utils.cpp
    src/cread.cpp
    src/pc.cpp
    src/lev.cpp
    src/eds.cpp
    src/pfs.cpp
    src/dfs.cpp
    src/rtpg.cpp
    src/rfl.cpp
    src/tpfc.cpp
    src/dtpfc.cpp
    src/dalg.cpp
    src/podem.cpp
    src/scoap.cpp
    src/tpg.cpp
    src/tpi.cpp
    src/ppa.cpp
    src/cop.cpp
)

# The algorithms live in a library so the unit tests can link them without
# dragging in the interactive main().
add_library(atpg_core STATIC ${CORE_SOURCES})
target_include_directories(atpg_core PUBLIC
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/core)

add_executable(simulator main.cpp)
target_link_libraries(simulator PRIVATE atpg_core)

target_compile_definitions(atpg_core PUBLIC $<$<CONFIG:Debug>:DEBUG>)


# Optional readline support
if(USE_READLINE)
    # Try to find readline
    find_path(READLINE_INCLUDE_DIR readline/readline.h
        HINTS /usr/include /usr/local/include /opt/homebrew/include /opt/local/include
    )
    find_library(READLINE_LIBRARY readline
        HINTS /usr/lib /usr/local/lib /opt/homebrew/lib /opt/local/lib
    )
    
    if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
        message(STATUS "GNU Readline found: ${READLINE_LIBRARY}")
        target_include_directories(simulator PRIVATE ${READLINE_INCLUDE_DIR})
        target_link_libraries(simulator PRIVATE ${READLINE_LIBRARY})
        target_compile_definitions(simulator PRIVATE USE_READLINE)
    else()
        message(STATUS "GNU Readline not found, line editing disabled")
    endif()
endif()


option(BUILD_TESTS "Build the unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
