#include <gtest/gtest.h>

// The `proto` library is intentionally built without sanitizers (its generated
// code must match the uninstrumented system libprotobuf). Reading protobuf
// containers from this ASan-instrumented binary otherwise trips false-positive
// container-overflow reports, so disable just that one check for the test binary.
extern "C" const char* __asan_default_options() { return "detect_container_overflow=0"; }

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
