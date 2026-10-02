#include "persist/save_writer.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <string>
#include <utility>

namespace lit::game {
SaveWriter::SaveWriter(SaveStore store)
    : store_{std::move(store)}, thread_{[this](std::stop_token stop) { run(stop); }} {}

void SaveWriter::submit(::lit::save::WorldSave save) {
    {
        const std::scoped_lock lock(mutex_);
        pending_ = std::move(save);  // a snapshot still waiting is older: dropped
    }
    wake_.notify_one();
}

SaveStats SaveWriter::stats() const {
    const std::scoped_lock lock(mutex_);
    return stats_;
}

void SaveWriter::run(std::stop_token stop) {
    for (;;) {
        ::lit::save::WorldSave save;
        {
            std::unique_lock lock(mutex_);
            // Woken by a snapshot, or by stop: one still waiting is written first.
            if (!wake_.wait(lock, stop, [this] { return pending_.has_value(); })) return;
            save = *std::move(pending_);
            pending_.reset();
        }
        const auto started = std::chrono::steady_clock::now();
        const std::string payload = save.SerializeAsString();
        const auto written = store_.write(payload);
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);

        const std::scoped_lock lock(mutex_);
        if (written) {
            ++stats_.written;
            stats_.last_bytes = payload.size();
            stats_.last_write_ms = static_cast<std::uint32_t>(took.count());
            spdlog::info("Saved the world: {} ({} bytes, {} ms)", written->string(), payload.size(),
                         took.count());
        } else {
            ++stats_.failed;
            spdlog::error("Saving the world failed: {}", written.error());
        }
    }
}
}  // namespace lit::game
