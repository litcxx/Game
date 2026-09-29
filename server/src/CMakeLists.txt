add_executable(server
  main.cpp
  asan_default_options.cpp
)

target_link_libraries(server PRIVATE
  net
  game
  shared
  third_party
  project_options
  project_warnings
)
