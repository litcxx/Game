find_package(OpenSSL REQUIRED)

add_library(crypto
  sha256.cpp
)

target_link_libraries(crypto PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
)

target_include_directories(crypto PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
)

target_include_directories(crypto PUBLIC
  ${CMAKE_SOURCE_DIR}/src/cryptography
)
