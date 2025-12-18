add_library(shared
  config/config.cpp
  protocol/opcodes.cpp
)

target_include_directories(shared PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
)

target_include_directories(shared PUBLIC
  ${CMAKE_SOURCE_DIR}/src/shared
)
