add_library(db
  connection/sql_connection.cpp 
)

target_link_libraries(db PRIVATE
  mariadb
)

target_include_directories(db PRIVATE
  # hardcoded for now
  /usr/include/mariadb
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/db
)
