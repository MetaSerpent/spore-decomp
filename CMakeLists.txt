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

# Linkable portable gameplay math extracted from real migrated SPTerrain functions.
# This is the first gameplay-oriented executable target in the source-first build.
add_library(spore_terrain_math STATIC src/runtime/TerraformMath.cpp)
target_include_directories(spore_terrain_math PUBLIC src)
target_compile_features(spore_terrain_math PUBLIC cxx_std_17)
add_executable(spore_terraform_probe tools/terraform_probe.cpp)
target_link_libraries(spore_terraform_probe PRIVATE spore_terrain_math)
# Deterministic, saveable terrain state built on the recovered brush functions.
add_library(spore_terrain_field_core STATIC src/runtime/TerraformField.cpp)
target_include_directories(spore_terrain_field_core PUBLIC src)
target_link_libraries(spore_terrain_field_core PUBLIC spore_terrain_math)
target_compile_features(spore_terrain_field_core PUBLIC cxx_std_17)
add_executable(spore_terrain_field tools/terrain_field.cpp)
target_link_libraries(spore_terrain_field PRIVATE spore_terrain_field_core)
# Conventional OBJ mesh export, derived from deterministic terrain height samples.
add_library(spore_terrain_mesh_core STATIC src/runtime/TerrainMesh.cpp)
target_include_directories(spore_terrain_mesh_core PUBLIC src)
target_link_libraries(spore_terrain_mesh_core PUBLIC spore_terrain_field_core)
# Recover two isolated animation-version functions from the migrated
# SP/CreatureAnim game binary objects (0x009A8170 and 0x009A81B0).
add_library(spore_recovered_creature_anim STATIC
            src/runtime/RecoveredCreatureAnim.cpp)
target_include_directories(spore_recovered_creature_anim PUBLIC src)
target_compile_features(spore_recovered_creature_anim PUBLIC cxx_std_17)
# Portable procedural creature morphology, gait and deterministic broadphase
# contact resolution. These modules are new BlackSpore code, NOT recovered
# Maxis skeletal/rigging or physics implementations.
add_library(spore_creature_runtime STATIC
  src/runtime/CreatureGeometry.cpp
  src/runtime/CreaturePhysics.cpp
  src/runtime/CreatureDynamics.cpp
  src/runtime/CreatureNavigation.cpp)
target_include_directories(spore_creature_runtime PUBLIC src)
target_compile_features(spore_creature_runtime PUBLIC cxx_std_17)
add_executable(spore_creature_probe tools/creature_probe.cpp)
target_link_libraries(spore_creature_probe PRIVATE spore_creature_runtime)
add_executable(spore_creature_route tools/creature_route.cpp)
target_link_libraries(spore_creature_route PRIVATE
                      spore_creature_runtime spore_terrain_field_core)

# CPU-only renderer: deterministic 3D projection/rasterization for any terrain.
add_library(spore_software_view STATIC src/runtime/SoftwareView.cpp)
target_include_directories(spore_software_view PUBLIC src)
target_link_libraries(spore_software_view PUBLIC
    spore_terrain_field_core spore_creature_runtime)
# Platform-native interactive viewer with CPU software rasterization.
# Windows uses GDI/Win32; Linux uses X11 when its development headers exist.
if(WIN32)
  add_library(spore_native_window STATIC src/runtime/NativeWindow.cpp)
  target_link_libraries(spore_native_window PUBLIC spore_software_view)
  target_link_libraries(spore_native_window PRIVATE user32 gdi32)
elseif(UNIX AND NOT APPLE)
  find_package(X11 QUIET)
  if(X11_FOUND)
    add_library(spore_native_window STATIC src/runtime/NativeWindow.cpp)
    target_link_libraries(spore_native_window PUBLIC spore_software_view)
    target_include_directories(spore_native_window PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(spore_native_window PRIVATE ${X11_LIBRARIES})
    target_compile_definitions(spore_native_window PRIVATE SPORE_NATIVE_X11=1)
  endif()
endif()
if(TARGET spore_native_window)
  target_include_directories(spore_native_window PUBLIC src)
  add_executable(spore_terrain_live tools/terrain_live.cpp)
  target_link_libraries(spore_terrain_live PRIVATE spore_native_window)
  add_executable(spore_creature_live tools/creature_live.cpp)
  target_link_libraries(spore_creature_live PRIVATE
                        spore_native_window spore_creature_runtime)
endif()
add_executable(spore_terrain_view tools/terrain_view.cpp)
target_link_libraries(spore_terrain_view PRIVATE spore_software_view)
add_executable(spore_terrain_mesh_cli tools/terrain_mesh.cpp)
set_target_properties(spore_terrain_mesh_cli PROPERTIES OUTPUT_NAME spore_terrain_mesh)
target_link_libraries(spore_terrain_mesh_cli PRIVATE spore_terrain_mesh_core)
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
add_executable(terraform_math_tests tests/terraform_math_tests.cpp)
target_link_libraries(terraform_math_tests PRIVATE spore_terrain_math)
add_test(NAME gameplay_terraform_math COMMAND terraform_math_tests)
add_test(NAME gameplay_terraform_probe
         COMMAND spore_terraform_probe type 0.8 0.75)
add_executable(terraform_field_tests tests/terraform_field_tests.cpp)
target_link_libraries(terraform_field_tests PRIVATE spore_terrain_field_core)
add_test(NAME gameplay_terrain_state_and_replay COMMAND terraform_field_tests)
add_test(NAME gameplay_terrain_field_smoke
         COMMAND spore_terrain_field run 16 12 6 42 0.3 0.4)
add_executable(terrain_mesh_tests tests/terrain_mesh_tests.cpp)
target_link_libraries(terrain_mesh_tests PRIVATE spore_terrain_mesh_core)
add_test(NAME gameplay_terrain_mesh_export COMMAND terrain_mesh_tests)
add_executable(software_view_tests tests/software_view_tests.cpp)
target_link_libraries(software_view_tests PRIVATE spore_software_view)
add_test(NAME gameplay_software_view_render_and_controls COMMAND software_view_tests)
add_executable(creature_view_tests tests/creature_view_tests.cpp)
target_link_libraries(creature_view_tests PRIVATE spore_software_view)
add_test(NAME gameplay_creature_mesh_depth_rendering COMMAND creature_view_tests)
add_executable(creature_runtime_tests tests/creature_runtime_tests.cpp)
target_link_libraries(creature_runtime_tests PRIVATE spore_creature_runtime)
add_executable(recovered_creature_anim_tests tests/recovered_creature_anim_tests.cpp)
target_link_libraries(recovered_creature_anim_tests PRIVATE spore_recovered_creature_anim)
add_test(NAME gameplay_recovered_creature_animation_version
         COMMAND recovered_creature_anim_tests)
add_test(NAME gameplay_creature_geometry_gait_and_contacts
         COMMAND creature_runtime_tests)
add_executable(creature_dynamics_tests tests/creature_dynamics_tests.cpp)
target_link_libraries(creature_dynamics_tests PRIVATE spore_creature_runtime)
add_test(NAME gameplay_creature_persistent_motion_replay
         COMMAND creature_dynamics_tests)
add_executable(creature_navigation_tests tests/creature_navigation_tests.cpp)
target_link_libraries(creature_navigation_tests PRIVATE
                      spore_creature_runtime spore_terrain_field_core)
add_test(NAME gameplay_creature_deterministic_navigation
         COMMAND creature_navigation_tests)
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
  # Run the real native X11 window and framebuffer blit loop under a virtual
  # display; no manual E2E interaction or Spore game assets are required.
  if(TARGET spore_terrain_live AND UNIX AND NOT APPLE)
    find_program(SPORE_XVFB_RUN xvfb-run)
    if(SPORE_XVFB_RUN)
      add_test(NAME gameplay_native_desktop_virtual_display
               COMMAND "${Python3_EXECUTABLE}"
                       "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_live_headless.py"
                       "$<TARGET_FILE:spore_terrain_field>"
                       "$<TARGET_FILE:spore_terrain_live>")
      if(TARGET spore_creature_live)
        add_test(NAME gameplay_native_creature_gait_virtual_display
                 COMMAND "${Python3_EXECUTABLE}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_creature_live.py"
                         "$<TARGET_FILE:spore_creature_live>")
      endif()
    endif()
  endif()
  add_test(NAME gameplay_creature_route_cli_blackbox
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_creature_route.py"
                   "$<TARGET_FILE:spore_terrain_field>"
                   "$<TARGET_FILE:spore_creature_route>")
  add_test(NAME gameplay_creature_probe_cli_blackbox
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_creature_probe.py"
                   "$<TARGET_FILE:spore_creature_probe>")
  add_test(NAME gameplay_terrain_field_cli_blackbox
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_terrain_field_cli.py"
                   "$<TARGET_FILE:spore_terrain_field>")
  add_test(NAME gameplay_terrain_mesh_cli_blackbox
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_terrain_mesh_cli.py"
                   "$<TARGET_FILE:spore_terrain_field>"
                   "$<TARGET_FILE:spore_terrain_mesh_cli>")
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
