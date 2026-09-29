#include <gtest/gtest.h>

// ASan defaults (detect_container_overflow=0) come from
// src/asan_default_options.cpp, compiled into this binary as into the server.

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
