add_executable(server 
  main.cpp
)

target_include_directories(server PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/net
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(server PRIVATE
  net
  game
  shared 
  db
)

target_compile_options(server PRIVATE
  $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -g -DDEBUG>
  $<$<CONFIG:Release>:-O2>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(server PRIVATE -fsanitize=address -fsanitize=undefined)
  target_link_options(server PRIVATE -fsanitize=address -fsanitize=undefined)
endif()
