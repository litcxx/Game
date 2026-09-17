add_library(crypto
  sha256.cpp
)

target_include_directories(crypto PUBLIC
  ${CMAKE_SOURCE_DIR}/src/cryptography
)

target_link_libraries(crypto PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  project_options
  project_warnings
)
