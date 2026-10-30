cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED SIMULATOR_EXE OR NOT EXISTS "${SIMULATOR_EXE}")
  message(FATAL_ERROR "Built simulator executable is required")
endif()
execute_process(
  COMMAND "${SIMULATOR_EXE}" list
  RESULT_VARIABLE rc OUTPUT_VARIABLE inventory ERROR_VARIABLE stderr
  TIMEOUT 5 OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT "${rc}" STREQUAL "0")
  message(FATAL_ERROR "Tool inventory failed: ${rc}")
endif()
string(JSON count ERROR_VARIABLE error LENGTH "${inventory}" tools)
if(NOT "${error}" STREQUAL "NOTFOUND" OR NOT count EQUAL 17)
  message(FATAL_ERROR "Tool inventory contract changed")
endif()
math(EXPR last "${count}-1")
foreach(i RANGE 0 ${last})
  string(JSON name GET "${inventory}" tools ${i} name)
  string(JSON domain GET "${inventory}" tools ${i} domain)
  if(name STREQUAL "" OR domain STREQUAL "")
    message(FATAL_ERROR "Empty name or domain at index ${i}")
  endif()
  execute_process(
    COMMAND "${SIMULATOR_EXE}" call "${name}" bounded_fixture
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE stderr
    TIMEOUT 5 OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${rc}" STREQUAL "0")
    message(FATAL_ERROR "Tool invocation failed: ${name}")
  endif()
  string(JSON ok ERROR_VARIABLE error GET "${output}" ok)
  if(NOT "${error}" STREQUAL "NOTFOUND" OR NOT ok)
    message(FATAL_ERROR "Invalid JSON/response for ${name}: ${error}")
  endif()
  string(JSON mode GET "${output}" mode)
  string(JSON actual_tool GET "${output}" tool)
  string(JSON actual_domain GET "${output}" domain)
  string(JSON resource GET "${output}" resource)
  string(JSON side_effects GET "${output}" side_effects)
  if(NOT mode STREQUAL "simulation_only" OR
     NOT actual_tool STREQUAL name OR NOT actual_domain STREQUAL domain OR
     NOT resource STREQUAL "bounded_fixture" OR side_effects)
    message(FATAL_ERROR "Read-only contract violation: ${name}")
  endif()
  string(LENGTH "${output}" output_bytes)
  if(output_bytes GREATER 2048)
    message(FATAL_ERROR "Unexpected oversized response from ${name}")
  endif()
endforeach()
string(REPEAT "x" 257 oversized)
foreach(resource IN ITEMS "" "unsafe space" "slash/path" "${oversized}")
  execute_process(
    COMMAND "${SIMULATOR_EXE}" call monitoring.health "${resource}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE stderr
    TIMEOUT 5 OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${rc}" STREQUAL "2")
    message(FATAL_ERROR "Malformed resource did not fail at native CLI boundary: ${rc}")
  endif()
  string(JSON ok ERROR_VARIABLE error GET "${output}" ok)
  string(JSON message GET "${output}" error)
  if(NOT "${error}" STREQUAL "NOTFOUND" OR ok OR
     NOT message STREQUAL "invalid_resource")
    message(FATAL_ERROR "Malformed resource response is not fail-closed")
  endif()
endforeach()
execute_process(
 COMMAND "${SIMULATOR_EXE}" call unknown.tool valid_fixture
 RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE stderr
 TIMEOUT 5 OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT "${rc}" STREQUAL "2")
  message(FATAL_ERROR "Unknown tool was not denied")
endif()
string(JSON message ERROR_VARIABLE error GET "${output}" error)
if(NOT "${error}" STREQUAL "NOTFOUND" OR NOT message STREQUAL "unknown_tool")
  message(FATAL_ERROR "Unknown tool error contract changed")
endif()
message(STATUS "Validated 17 real executable tool contracts and 5 negative boundary inputs")
