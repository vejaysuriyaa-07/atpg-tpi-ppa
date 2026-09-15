# Prefer an installed GoogleTest; fall back to fetching it if the machine
# does not have one.
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(unit_tests
    test_cop.cpp
    test_ppa.cpp
    test_tpi.cpp
)

target_link_libraries(unit_tests PRIVATE atpg_core GTest::gtest GTest::gtest_main)
target_compile_definitions(unit_tests PRIVATE
    CKT_DIR="${CMAKE_SOURCE_DIR}/ckts")

include(GoogleTest)
gtest_discover_tests(unit_tests)
