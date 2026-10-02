#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "game/v1/protocol.pb.h"

namespace lit::game {
// Durations over one period, in microseconds.
struct TimingStats {
    std::uint32_t avg_us{0};  // rounded down
    std::uint32_t p99_us{0};  // nearest rank
    std::uint32_t max_us{0};
};

// The round trips the players' clients reported (Ping.rtt_ms), in milliseconds.
struct RoundTripStats {
    std::uint32_t p50_ms{0};  // nearest rank
    std::uint32_t p95_ms{0};
    std::uint32_t max_ms{0};
};

// What the server did over one period — logged as one `metrics` line (GAME-012).
struct MetricsReport {
    std::uint32_t period_s{0};
    std::uint32_t ccu{0};  // players in the world at the end of the period
    std::uint32_t ticks{0};
    TimingStats tick;  // every tick
    std::uint32_t snapshot_ticks{0};
    TimingStats snapshot_tick;   // just the ticks that send snapshots
    std::uint64_t snapshots{0};  // one per recipient
    std::uint32_t snapshot_bytes_avg{0};
    std::uint32_t drops{0};          // frames refused by a full send queue
    std::uint32_t resyncs{0};        // resync snapshots sent after a drop
    std::uint32_t closed_behind{0};  // sessions closed for falling behind again
    std::uint32_t joins{0};          // characters entering the world: new, or back after the grace
    std::uint32_t leaves{0};         // characters leaving it: their reconnect grace is over
    std::uint32_t resumes{0};        // sessions taking over a character still in the world
    // What the players' clients reported in their Pings (PT-0).
    RoundTripStats rtt;
    std::uint32_t corrections{0};     // prediction corrections, over the client's threshold
    std::uint32_t max_correction{0};  // the largest, world units
    // What happened in the game.
    std::uint32_t spawns{0};                      // spawns and respawns
    std::uint32_t deaths{0};                      // characters killed
    std::uint32_t captures{0};                    // cells that changed owner
    std::map<std::string, std::uint32_t> errors;  // ServerErrors sent, by code name
    // Saving the world (GAME-019).
    std::uint32_t save_snapshots{0};        // snapshots handed to the writer this period
    std::uint32_t save_snapshot_us_max{0};  // the longest, on the game thread
    std::uint32_t saves_written{0};         // the writer's, since the start
    std::uint32_t saves_failed{0};
    std::uint64_t save_bytes{0};     // the last written file's size
    std::uint32_t save_write_ms{0};  // how long writing it took
};

// Counts what the World does over one period. Game thread only.
class Metrics {
  public:
    void record_tick(std::uint32_t duration_us, bool snapshot_tick);
    void record_snapshot(std::size_t bytes);
    void record_drop() { ++current_.drops; }
    void record_resync() { ++current_.resyncs; }
    void record_closed_behind() { ++current_.closed_behind; }
    void record_join() { ++current_.joins; }
    void record_leave() { ++current_.leaves; }
    void record_resume() { ++current_.resumes; }
    // A client's report from its Ping: its last round trip (0: not measured yet,
    // not a sample) and the prediction corrections since its previous Ping.
    void record_client_report(std::uint32_t rtt_ms, std::uint32_t corrections,
                              std::uint32_t max_correction);
    void record_spawn() { ++current_.spawns; }
    void record_deaths(std::uint32_t n) { current_.deaths += n; }
    void record_captures(std::uint32_t n) { current_.captures += n; }
    // A world snapshot taken for the save, `us` on the game thread.
    void record_save_snapshot(std::uint32_t us) {
        ++current_.save_snapshots;
        current_.save_snapshot_us_max = std::max(current_.save_snapshot_us_max, us);
    }
    // Counted under the code's name without its prefix: "RATE_LIMITED".
    void record_error(::game::v1::ErrorCode code);

    // The period's report; the next period starts from zero.
    MetricsReport report(std::uint32_t period_s, std::uint32_t ccu);

  private:
    MetricsReport current_;                   // the counters
    std::vector<std::uint32_t> tick_us_;      // every tick's duration
    std::vector<std::uint32_t> snapshot_us_;  // the snapshot ticks' durations
    std::uint64_t snapshot_bytes_{0};
    std::vector<std::uint32_t> rtt_ms_;  // every measured round trip reported
};

// One line of JSON: {"period_s":60,"ccu":3,"ticks":3600,"tick_us":{"avg":..,"p99":..,"max":..},..}
std::string to_json(const MetricsReport& report);
}  // namespace lit::game
