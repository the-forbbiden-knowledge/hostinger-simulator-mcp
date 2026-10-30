cmake_minimum_required(VERSION 3.24)
project(hostinger_simulator_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simulator_core STATIC src/native/simulator.cpp src/native/agent_security.cpp src/native/recon_security.cpp)
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

add_executable(agent_security_tests tests/agent_security_tests.cpp)
target_link_libraries(agent_security_tests PRIVATE simulator_core)
add_test(NAME agent_policy_unit COMMAND agent_security_tests)
add_test(NAME security_scenario_discovery COMMAND simulator security list)
add_test(NAME security_untrusted_denied COMMAND simulator security evaluate document execute_tool reviewer same_session approved)
add_test(NAME security_guarded_call COMMAND simulator security call trusted_application operator same_session approved monitoring.health demo)

add_executable(recon_security_tests tests/recon_security_tests.cpp)
target_link_libraries(recon_security_tests PRIVATE simulator_core)
add_test(NAME recon_policy_unit COMMAND recon_security_tests)
add_test(NAME recon_inventory COMMAND simulator recon list)
add_test(NAME recon_restricted_denial COMMAND simulator recon inspect rag_source_attribution viewer)
add_test(NAME recon_authorized_preview COMMAND simulator recon inspect rag_source_attribution reviewer)
