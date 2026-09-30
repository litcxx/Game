include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.tar.gz
    URL_HASH SHA256=7315acb6bf10e99f332c8a43f00d5fbb1ee6ca48c52f6b936991b216c586aaad
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(SOURCES
  main.cpp
  ${CMAKE_SOURCE_DIR}/src/asan_default_options.cpp
  shared/config_tests.cpp
  shared/session_token_tests.cpp
  shared/ts_queue_tests.cpp
  game/spatial/geometry_tests.cpp
  game/spatial/spatial_index_tests.cpp
  game/systems/capital_system_tests.cpp
  game/systems/combat_system_tests.cpp
  game/systems/damage_tests.cpp
  game/systems/input_system_tests.cpp
  game/systems/join_system_tests.cpp
  game/systems/presence_system_tests.cpp
  game/systems/projectile_system_tests.cpp
  game/systems/spawn_system_tests.cpp
  game/systems/vision_system_tests.cpp
  game/sync/delivery_tests.cpp
  game/world/connection_tests.cpp
  game/world/metrics_tests.cpp
  game/world/world_tests.cpp
)

add_executable(unit_tests
  ${SOURCES}
)

target_link_libraries(unit_tests PRIVATE
  GTest::gtest
  GTest::gtest_main
  GTest::gmock
  net
  game
  shared
  third_party
  project_options
  project_warnings
)

# The ShippedConfig tests check the combat balance of the config the server ships.
target_compile_definitions(unit_tests PRIVATE
  LIT_SHIPPED_CONFIG="${CMAKE_SOURCE_DIR}/config/config.json"
)

# net/game/shared expose their headers via PUBLIC usage requirements, so only
# the test-local include directory needs to be listed explicitly.
target_include_directories(unit_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/test/unit
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
