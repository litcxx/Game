set(SOURCES
  main.cpp
)

add_executable(server 
  ${SOURCES}
)

target_compile_options(server PRIVATE
  $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -g -DDEBUG>
  $<$<CONFIG:Release>:-O2>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(server PRIVATE -fsanitize=address -fsanitize=undefined)
  target_link_options(server PRIVATE -fsanitize=address -fsanitize=undefined)
endif()
