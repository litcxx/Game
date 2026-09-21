add_executable(server
  main.cpp
)

# Include dirs for net/game/shared/db come transitively from their PUBLIC
# usage requirements below, so they don't need to be listed again here.
target_link_libraries(server PRIVATE
  net
  game
  shared
  db
  third_party
  project_options
  project_warnings
)
