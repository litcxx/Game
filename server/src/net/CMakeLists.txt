add_library(net
  server/server.cpp
  session/session.cpp
  session/packet_handler.cpp
  socket/wss_socket.cpp
  socket/ws_socket.cpp
)

target_include_directories(net PUBLIC
  ${CMAKE_SOURCE_DIR}/src/net
)

target_link_libraries(net PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  shared
  third_party
  project_options
  project_warnings
)
