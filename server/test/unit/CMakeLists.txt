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
  net/protocol/net_packet_tests.cpp
  net/session/packet_handler_tests.cpp
  shared/ts_queue_tests.cpp
  game/player/player_tests.cpp
  game/world/world_tests.cpp
  game/physics/collision_tests.cpp
  game/physics/swept_math_tests.cpp
  cryptography/sha256_tests.cpp
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
  crypto
  project_options
  project_warnings
)

# net/game/shared expose their headers via PUBLIC usage requirements, so only
# the test-local include directory needs to be listed explicitly.
target_include_directories(unit_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/test/unit
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
