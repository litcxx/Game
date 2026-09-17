add_library(game
  player/player.cpp
  world/world.cpp
  physics/collision.cpp
  physics/swept_math.cpp
  tile/tile.cpp
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
