add_library(db
  connection/sql_connection.cpp 
  database/login_database.cpp
)

target_link_libraries(db PRIVATE
  mariadb
  crypto
)

target_include_directories(db PRIVATE 
  # hardcoded for now
  /usr/include/mariadb
  ${CMAKE_SOURCE_DIR}/third_party
)

target_include_directories(db PUBLIC
  ${CMAKE_SOURCE_DIR}/src/db
)
