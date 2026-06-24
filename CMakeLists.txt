cmake_minimum_required(VERSION 3.10)
project(CPUEmulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(emulator
    src/main.cpp
    src/cpu.cpp
    src/memory.cpp
)

add_executable(cpu_tests
    tests/cpu_tests.cpp
    src/cpu.cpp
    src/memory.cpp
)

target_compile_options(emulator PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_options(cpu_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

enable_testing()
add_test(NAME cpu_tests COMMAND cpu_tests)
