add_library(game
  spatial/spatial_index.cpp
  systems/capture_system.cpp
  systems/combat_system.cpp
  systems/damage.cpp
  systems/input_system.cpp
  systems/movement_system.cpp
  systems/spawn_system.cpp
  sync/messages.cpp
  sync/snapshot_builder.cpp
  world/world.cpp
)

target_include_directories(game PUBLIC
  ${CMAKE_SOURCE_DIR}/src/game
)

target_link_libraries(game PRIVATE
  shared
  third_party
  project_options
  project_warnings
)
