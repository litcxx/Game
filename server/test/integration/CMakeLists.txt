# These are standalone programs that connect to a real MariaDB instance and
# take a config file as argv[1], so they are only registered with CTest when
# explicitly requested (a live database must be available).
option(RUN_DB_INTEGRATION_TESTS "Register DB integration tests with CTest (requires a live database)" OFF)
set(DB_INTEGRATION_TEST_CONFIG "${CMAKE_SOURCE_DIR}/config/config.json"
    CACHE FILEPATH "Config file passed to DB integration tests")

function(add_db_integration_test name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE
    db
    shared
    third_party
    project_options
    project_warnings
  )
  if(RUN_DB_INTEGRATION_TESTS)
    add_test(NAME ${name} COMMAND ${name} "${DB_INTEGRATION_TEST_CONFIG}")
    set_tests_properties(${name} PROPERTIES LABELS "integration")
  endif()
endfunction()

# TEMP: the auth/DB module is parked for the MVP (no authorization yet) and the
# accounts config was removed from Config. These tests reference the removed
# accounts_db_config and no longer compile. Re-enable together with auth.
# add_db_integration_test(sql_connection_tests db/connection/sql_connection_tests.cpp)
# add_db_integration_test(login_database_tests db/database/login_database_tests.cpp)
