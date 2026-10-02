include(FetchContent)

# SYSTEM marks the dependencies' headers as system includes, so our strict
# warnings (-Wall -Wextra -Werror ...) never fire on third-party code.

# json.tar.xz: the release's own archive for FetchContent (the library and its
# CMake files, without the tests and docs).
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

# 1.15 bundles fmt 11: the fmt 9 in 1.12 trips a false -Warray-bounds in GCC 13
# at -O3 (fmt's bigint, inlined), which -Werror turns into a failed Release build.
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
    URL_HASH SHA256=15a04e69c222eb6c01094b5c7ff8a249b36bb22788d72519646fb85feb267e67
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
FetchContent_MakeAvailable(spdlog)

add_library(third_party INTERFACE)
target_link_libraries(third_party INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
