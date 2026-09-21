include(FetchContent)

# Some pinned dependencies (e.g. nlohmann/json v3.11.0) still declare
# cmake_minimum_required(VERSION < 3.5), which CMake >= 4.0 rejects. Provide the
# documented compatibility floor so these third-party projects still configure.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# SYSTEM marks the dependencies' headers as system includes, so our strict
# warnings (-Wall -Wextra -Werror ...) never fire on third-party code.
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.0.tar.gz
    URL_HASH SHA256=e0c4fbd03c0bb7e99b40791e0276be61e5f531106e1486e8f0d771a7ed6d754a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz
    URL_HASH SHA256=4dccf2d10f410c1e2feaff89966bfc49a1abb29ef6f08246335b110e001e09a9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
FetchContent_MakeAvailable(spdlog)

add_library(third_party INTERFACE)
target_link_libraries(third_party INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
