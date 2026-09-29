# libcrypto: the session token's randomness (RAND_bytes) and its SHA-256.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(shared
  config/config.cpp
  utils/session_token.cpp
)

target_include_directories(shared PUBLIC
  ${CMAKE_SOURCE_DIR}/src/shared
)

target_link_libraries(shared
  PUBLIC
    proto
  PRIVATE
    OpenSSL::Crypto
    third_party
    project_options
    project_warnings
)
