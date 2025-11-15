add_library(game
  player/player.cpp
  world/world.cpp
)

target_include_directories(game PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/shared
)
