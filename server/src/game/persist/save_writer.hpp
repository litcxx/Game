#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

#include "persist/save_sink.hpp"
#include "persist/save_store.hpp"
#include "save.pb.h"

namespace lit::game {
// Writes the World's snapshots on a thread of its own (GDD R14: no I/O in the
// tick): serializes each and hands it to the SaveStore. A snapshot waiting when
// a newer one comes is dropped — the newer has all it had. A failed write is
// logged and counted; the game goes on and the next snapshot tries again.
// Stopping (the destructor) still writes the snapshot waiting: the one the World
// takes on SIGTERM.
class SaveWriter final : public ISaveSink {
  public:
    explicit SaveWriter(SaveStore store);
    SaveWriter(const SaveWriter&) = delete;
    SaveWriter& operator=(const SaveWriter&) = delete;
    ~SaveWriter() override = default;  // the thread (last member) writes what waits, then stops

    void submit(::lit::save::WorldSave save) override;
    SaveStats stats() const override;

  private:
    void run(std::stop_token stop);

    SaveStore store_;  // the writer thread's alone
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::optional<::lit::save::WorldSave> pending_;  // under mutex_
    SaveStats stats_;                                // under mutex_
    std::jthread thread_;                            // last: stops (and joins) first
};
}  // namespace lit::game
