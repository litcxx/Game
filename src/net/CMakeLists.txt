find_package(OpenSSL REQUIRED)

add_library(net
  server/server.cpp
  session/session.cpp
  # protocol/base_packet.cpp
)

target_include_directories(net PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/src/net
)

target_link_libraries(net PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
)
