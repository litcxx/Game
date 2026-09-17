add_library(shared
  config/config.cpp
  protocol/net_packet.cpp
)

target_include_directories(shared PUBLIC
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(shared PRIVATE
  third_party
  project_options
  project_warnings
)
