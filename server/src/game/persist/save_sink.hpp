#pragma once

#include <cstdint>

#include "save.pb.h"

namespace lit::game {
// How the world's saving went since the server started (for the metrics line).
struct SaveStats {
    std::uint32_t written{0};        // saves written
    std::uint32_t failed{0};         // saves that could not be written
    std::uint64_t last_bytes{0};     // the last written file's size
    std::uint32_t last_write_ms{0};  // how long writing it took
};

// Where the World hands its snapshots (GAME-019). Implemented by SaveWriter,
// faked in tests.
class ISaveSink {
  public:
    virtual ~ISaveSink() = default;
    virtual void submit(::lit::save::WorldSave save) = 0;
    virtual SaveStats stats() const = 0;
};
}  // namespace lit::game
