cmake_minimum_required(VERSION 3.20)
project(spore_decomp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(spore_resource
  src/resource/DBPF.cpp
  src/resource/RefPack.cpp
  src/resource/Hash.cpp
  src/resource/PackageStack.cpp
  src/resource/PropertyList.cpp
  src/resource/PropertyResolver.cpp
  src/resource/PropertyDatabase.cpp)
target_include_directories(spore_resource PUBLIC src)
if(MSVC)
  target_compile_options(spore_resource PRIVATE /W4)
else()
  target_compile_options(spore_resource PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dbpf_tool tools/dbpf_tool.cpp)
target_link_libraries(dbpf_tool PRIVATE spore_resource)

# Asset-free executable for quantifying progress of our .prop decoding coverage.
add_executable(spore_prop_audit tools/prop_audit.cpp)
target_link_libraries(spore_prop_audit PRIVATE spore_resource)

enable_testing()
add_executable(resource_tests tests/resource_tests.cpp)
target_link_libraries(resource_tests PRIVATE spore_resource)
add_test(NAME resource_unit COMMAND resource_tests)
# Cross-package asset precedence: isolated, synthetic DBPF fixtures.
add_executable(package_stack_tests tests/package_stack_tests.cpp)
target_link_libraries(package_stack_tests PRIVATE spore_resource)
add_test(NAME resource_package_stack COMMAND package_stack_tests)
# PropertyList parser based on the documented binary .prop subset, synthetic fixtures only.
add_executable(property_list_tests tests/property_list_tests.cpp)
target_link_libraries(property_list_tests PRIVATE spore_resource)
add_test(NAME resource_property_list COMMAND property_list_tests)
add_executable(property_resolver_tests tests/property_resolver_tests.cpp)
target_link_libraries(property_resolver_tests PRIVATE spore_resource)
add_test(NAME resource_property_inheritance COMMAND property_resolver_tests)
# A memoized, typed configuration manager with deterministic package priorities.
add_executable(property_database_tests tests/property_database_tests.cpp)
target_link_libraries(property_database_tests PRIVATE spore_resource)
add_test(NAME resource_property_database COMMAND property_database_tests)
add_executable(property_fuzz_tests tests/property_fuzz_tests.cpp)
target_link_libraries(property_fuzz_tests PRIVATE spore_resource)
add_test(NAME resource_property_mutations COMMAND property_fuzz_tests)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME resource_property_audit_blackbox
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_prop_audit.py"
                   "$<TARGET_FILE:spore_prop_audit>")
endif()
# Corpus tests need the user's own game data; point SPORE_DATA at Spore/Data.
if(DEFINED ENV{SPORE_DATA})
  add_test(NAME resource_corpus COMMAND resource_tests --corpus $ENV{SPORE_DATA})
endif()

# Shared C ABI for tools/difftest (differential tests against the original code).
add_library(spore_resource_c SHARED src/resource/capi.cpp)
target_link_libraries(spore_resource_c PRIVATE spore_resource)
set_target_properties(spore_resource PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Differential tests against the original code; need the local analysis image
# (tools/steamstub_analysis_image.py) and Python with unicorn+pefile (.venv).
set(SPORE_IMAGE "${CMAKE_SOURCE_DIR}/work/SporeApp.analysis.bin")
set(SPORE_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python")
if(EXISTS "${SPORE_IMAGE}" AND EXISTS "${SPORE_PYTHON}")
  add_test(NAME resource_difftest
           COMMAND ${SPORE_PYTHON} ${CMAKE_SOURCE_DIR}/tools/difftest/test_resource.py
                   ${SPORE_IMAGE} $<TARGET_FILE:spore_resource_c> $ENV{SPORE_DATA})
endif()
