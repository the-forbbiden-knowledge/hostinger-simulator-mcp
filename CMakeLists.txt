cmake_minimum_required(VERSION 3.24)
project(hostinger_simulator_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simulator_core STATIC src/native/simulator.cpp)
target_include_directories(simulator_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(simulator_core PRIVATE /W4 /permissive- /sdl /EHsc)
else()
  target_compile_options(simulator_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(simulator src/native/main.cpp)
target_link_libraries(simulator PRIVATE simulator_core)

enable_testing()
add_executable(simulator_tests tests/native_tests.cpp)
target_link_libraries(simulator_tests PRIVATE simulator_core)
add_test(NAME native_unit COMMAND simulator_tests)
add_test(NAME cli_discovery COMMAND simulator list)
add_test(NAME cli_health COMMAND simulator call monitoring.health example)
