add_executable(server
  main.cpp
)

target_link_libraries(server PRIVATE
  net
  game
  shared
  # db
  third_party
  project_options
  project_warnings
)
