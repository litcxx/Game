find_package(OpenSSL REQUIRED)

add_library(net
  server/server.cpp
  session/session.cpp
  session/packet_handler.cpp
  socket/wss_socket.cpp
  socket/ws_socket.cpp
)

target_include_directories(net PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
)

target_include_directories(net PUBLIC
  ${CMAKE_SOURCE_DIR}/src/net
)

target_link_libraries(net PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  shared 
)
