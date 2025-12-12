add_library(shared
  config/config.cpp
  protocol/opcodes.cpp
  db/sql_connection.cpp
)

target_include_directories(shared PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/game
)
