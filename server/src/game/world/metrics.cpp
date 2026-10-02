#include "metrics.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lit::game {
namespace {
// The `pct`-th percentile of sorted, non-empty `values`, by nearest rank.
std::uint32_t nearest_rank(const std::vector<std::uint32_t>& values, std::uint64_t pct) {
    const std::uint64_t n = values.size();
    const std::uint64_t rank = (pct * n + 99) / 100;  // ceil(pct / 100 * n), >= 1
    return values[rank - 1];
}

// Sorts `us` in place.
TimingStats timing_of(std::vector<std::uint32_t>& us) {
    if (us.empty()) return {};
    std::ranges::sort(us);
    const std::uint64_t sum = std::accumulate(us.begin(), us.end(), std::uint64_t{0});
    return {static_cast<std::uint32_t>(sum / us.size()), nearest_rank(us, 99), us.back()};
}

// Sorts `ms` in place.
RoundTripStats round_trips_of(std::vector<std::uint32_t>& ms) {
    if (ms.empty()) return {};
    std::ranges::sort(ms);
    return {nearest_rank(ms, 50), nearest_rank(ms, 95), ms.back()};
}
}  // namespace

void Metrics::record_tick(std::uint32_t duration_us, bool snapshot_tick) {
    tick_us_.push_back(duration_us);
    if (snapshot_tick) snapshot_us_.push_back(duration_us);
}

void Metrics::record_snapshot(std::size_t bytes) {
    ++current_.snapshots;
    snapshot_bytes_ += bytes;
}

void Metrics::record_client_report(std::uint32_t rtt_ms, std::uint32_t corrections,
                                   std::uint32_t max_correction) {
    if (rtt_ms != 0) rtt_ms_.push_back(rtt_ms);
    current_.corrections += corrections;
    current_.max_correction = std::max(current_.max_correction, max_correction);
}

void Metrics::record_error(::game::v1::ErrorCode code) {
    constexpr std::string_view kPrefix = "ERROR_CODE_";
    std::string name{::game::v1::ErrorCode_Name(code)};
    if (name.starts_with(kPrefix)) name.erase(0, kPrefix.size());
    ++current_.errors[name];
}

MetricsReport Metrics::report(std::uint32_t period_s, std::uint32_t ccu) {
    MetricsReport r = std::exchange(current_, {});
    r.period_s = period_s;
    r.ccu = ccu;
    r.ticks = static_cast<std::uint32_t>(tick_us_.size());
    r.tick = timing_of(tick_us_);
    r.snapshot_ticks = static_cast<std::uint32_t>(snapshot_us_.size());
    r.snapshot_tick = timing_of(snapshot_us_);
    r.snapshot_bytes_avg =
        r.snapshots == 0 ? 0 : static_cast<std::uint32_t>(snapshot_bytes_ / r.snapshots);
    r.rtt = round_trips_of(rtt_ms_);

    tick_us_.clear();  // keeps the capacity: no allocations next period
    snapshot_us_.clear();
    snapshot_bytes_ = 0;
    rtt_ms_.clear();
    return r;
}

std::string to_json(const MetricsReport& report) {
    const auto timing = [](const TimingStats& t) {
        return nlohmann::ordered_json{{"avg", t.avg_us}, {"p99", t.p99_us}, {"max", t.max_us}};
    };
    // Insertion order kept: the line reads like the report.
    const nlohmann::ordered_json j{
        {"period_s", report.period_s},
        {"ccu", report.ccu},
        {"ticks", report.ticks},
        {"tick_us", timing(report.tick)},
        {"snapshot_ticks", report.snapshot_ticks},
        {"snapshot_tick_us", timing(report.snapshot_tick)},
        {"snapshots", report.snapshots},
        {"snapshot_bytes_avg", report.snapshot_bytes_avg},
        {"drops", report.drops},
        {"resyncs", report.resyncs},
        {"closed_behind", report.closed_behind},
        {"joins", report.joins},
        {"leaves", report.leaves},
        {"resumes", report.resumes},
        {"rtt_ms",
         {{"p50", report.rtt.p50_ms}, {"p95", report.rtt.p95_ms}, {"max", report.rtt.max_ms}}},
        {"corrections", {{"count", report.corrections}, {"max", report.max_correction}}},
        {"spawns", report.spawns},
        {"deaths", report.deaths},
        {"captures", report.captures},
        {"errors", nlohmann::ordered_json(report.errors)},
        {"save",
         {{"snapshots", report.save_snapshots},
          {"snapshot_us_max", report.save_snapshot_us_max},
          {"written", report.saves_written},
          {"failed", report.saves_failed},
          {"bytes", report.save_bytes},
          {"write_ms", report.save_write_ms}}},
    };
    return j.dump();
}
}  // namespace lit::game
