# sql_connection test
add_executable(sql_connection_tests
  db/connection/sql_connection_tests.cpp
)
target_link_libraries(sql_connection_tests PRIVATE 
  db 
  shared
)
target_include_directories(sql_connection_tests PRIVATE
  # ${CMAKE_SOURCE_DIR}/src/db
  # ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/third_party
)

# login_database test
add_executable(login_database_tests
  db/database/login_database_tests.cpp
)
target_link_libraries(login_database_tests PRIVATE 
  db 
  shared
)
target_include_directories(login_database_tests PRIVATE
  # ${CMAKE_SOURCE_DIR}/src/db
  # ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/third_party
)
