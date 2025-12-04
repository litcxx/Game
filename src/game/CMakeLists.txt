add_library(game
  player/player.cpp
  world/world.cpp
  physics/collision.cpp
  tile/tile.cpp
)

target_include_directories(game PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(game PRIVATE
  shared 
)
