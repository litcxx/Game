add_library(net
  server/server.cpp
  session/session.cpp
)

target_include_directories(net PUBLIC
  ${CMAKE_SOURCE_DIR}/src/net
)

target_link_libraries(net PRIVATE
  shared
  third_party
  project_options
  project_warnings
)
