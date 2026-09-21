find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb)

add_library(db
  connection/sql_connection.cpp
  database/login_database.cpp
)

target_include_directories(db PUBLIC
  ${CMAKE_SOURCE_DIR}/src/db
)

target_link_libraries(db PRIVATE
  PkgConfig::MARIADB
  crypto
  third_party
  project_options
  project_warnings
)
