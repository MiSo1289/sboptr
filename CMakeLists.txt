cmake_minimum_required(VERSION 3.13)
project(SboPtr VERSION 0.1)

enable_testing()

find_package(Catch2 CONFIG REQUIRED)

add_library(sboptr INTERFACE)
add_library(sboptr::sboptr ALIAS sboptr)

target_compile_features(sboptr INTERFACE cxx_std_17)
target_include_directories(sboptr INTERFACE include)
target_sources(sboptr INTERFACE include/sboptr/sboptr.hpp)

add_executable(sboptr_tests)
target_compile_definitions(sboptr_tests PRIVATE CATCH_CONFIG_MAIN)
target_sources(sboptr_tests PRIVATE tests/sboptr/sboptr_tests.cpp)
target_link_libraries(
    sboptr_tests 
    PRIVATE 
        sboptr::sboptr
        Catch2::Catch2
)

add_test(NAME sboptr_tests COMMAND sboptr_tests)

add_executable(sboptr_example)
target_sources(sboptr_example PRIVATE examples/example.cpp)
target_link_libraries(sboptr_example PRIVATE sboptr::sboptr)