add_library(shared
  config/config.cpp
)

target_include_directories(shared PUBLIC
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(shared
  PUBLIC
    proto
  PRIVATE
    third_party
    project_options
    project_warnings
)
