// AddressSanitizer defaults for our executables — the server and the unit tests
// both compile this file in (the ASan runtime looks the function up in the
// executable; a copy in a static library would not be linked). ASAN_OPTIONS
// still overrides it.
//
// detect_container_overflow=0: libprotobuf (and the `proto` library, see
// proto/CMakeLists.txt) is built without ASan, while protobuf's headers (v34 and
// newer) annotate RepeatedField buffers for ASan in our instrumented code only.
// The two disagree about which bytes are in use, and ASan reports container
// overflows that are not there — e.g. on the server's first ErrorCode_Name().
// Nothing of ours relies on the check: libstdc++ annotates std::vector only with
// _GLIBCXX_SANITIZE_VECTOR.
// The name is the ASan runtime's hook, reserved identifier and all.
// NOLINTNEXTLINE(bugprone-reserved-identifier,readability-identifier-naming)
extern "C" const char* __asan_default_options() { return "detect_container_overflow=0"; }
