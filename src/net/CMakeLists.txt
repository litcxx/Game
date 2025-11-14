find_package(OpenSSL REQUIRED)

add_library(net
  server/server.cpp
  session/session.cpp
  socket/wss_socket.cpp
)

target_include_directories(net PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/net
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(net PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
)
