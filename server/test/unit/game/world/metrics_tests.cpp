#include <gtest/gtest.h>

#include <cstdint>
#include <nlohmann/json.hpp>

#include "game/v1/protocol.pb.h"
#include "world/metrics.hpp"

TEST(Metrics, AnEmptyPeriodReportsZeros) {
    lit::game::Metrics metrics;

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.period_s, 60u);
    EXPECT_EQ(r.ccu, 0u);
    EXPECT_EQ(r.ticks, 0u);
    EXPECT_EQ(r.tick.avg_us, 0u);
    EXPECT_EQ(r.tick.p99_us, 0u);
    EXPECT_EQ(r.tick.max_us, 0u);
    EXPECT_EQ(r.snapshots, 0u);
    EXPECT_EQ(r.snapshot_bytes_avg, 0u);
    EXPECT_EQ(r.rtt.p50_ms, 0u);
    EXPECT_EQ(r.rtt.p95_ms, 0u);
    EXPECT_EQ(r.rtt.max_ms, 0u);
    EXPECT_EQ(r.corrections, 0u);
    EXPECT_EQ(r.max_correction, 0u);
    EXPECT_EQ(r.spawns, 0u);
    EXPECT_EQ(r.deaths, 0u);
    EXPECT_EQ(r.captures, 0u);
    EXPECT_TRUE(r.errors.empty());
}

TEST(Metrics, TickTimesGiveAverageP99AndMax) {
    lit::game::Metrics metrics;
    for (std::uint32_t us = 100; us >= 1; --us) metrics.record_tick(us, false);  // any order

    const auto r = metrics.report(60, 3);

    EXPECT_EQ(r.ccu, 3u);
    EXPECT_EQ(r.ticks, 100u);
    EXPECT_EQ(r.tick.avg_us, 50u);  // 50.5, rounded down
    EXPECT_EQ(r.tick.p99_us, 99u);  // nearest rank: the 99th of 100
    EXPECT_EQ(r.tick.max_us, 100u);
}

TEST(Metrics, SnapshotTicksAreAlsoTimedApart) {
    lit::game::Metrics metrics;
    metrics.record_tick(10, false);
    metrics.record_tick(40, true);
    metrics.record_tick(20, false);
    metrics.record_tick(60, true);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.ticks, 4u);
    EXPECT_EQ(r.tick.max_us, 60u);
    EXPECT_EQ(r.snapshot_ticks, 2u);
    EXPECT_EQ(r.snapshot_tick.avg_us, 50u);
    EXPECT_EQ(r.snapshot_tick.p99_us, 60u);
    EXPECT_EQ(r.snapshot_tick.max_us, 60u);
}

TEST(Metrics, SnapshotBytesAreAveragedPerSnapshot) {
    lit::game::Metrics metrics;
    metrics.record_snapshot(100);
    metrics.record_snapshot(300);
    metrics.record_snapshot(201);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.snapshots, 3u);
    EXPECT_EQ(r.snapshot_bytes_avg, 200u);  // 601 / 3
}

TEST(Metrics, CountsDeliveryPresenceAndErrorsByCode) {
    lit::game::Metrics metrics;
    metrics.record_drop();
    metrics.record_drop();
    metrics.record_resync();
    metrics.record_closed_behind();
    metrics.record_join();
    metrics.record_join();
    metrics.record_leave();
    metrics.record_resume();
    metrics.record_resume();
    metrics.record_resume();
    metrics.record_error(::game::v1::ERROR_CODE_RATE_LIMITED);
    metrics.record_error(::game::v1::ERROR_CODE_SPAWN_TOO_EARLY);
    metrics.record_error(::game::v1::ERROR_CODE_SPAWN_TOO_EARLY);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.drops, 2u);
    EXPECT_EQ(r.resyncs, 1u);
    EXPECT_EQ(r.closed_behind, 1u);
    EXPECT_EQ(r.joins, 2u);
    EXPECT_EQ(r.leaves, 1u);
    EXPECT_EQ(r.resumes, 3u);
    ASSERT_EQ(r.errors.size(), 2u);
    EXPECT_EQ(r.errors.at("RATE_LIMITED"), 1u);
    EXPECT_EQ(r.errors.at("SPAWN_TOO_EARLY"), 2u);
}

TEST(Metrics, TheClientsRoundTripsGiveP50P95AndMax) {
    lit::game::Metrics metrics;
    for (std::uint32_t ms = 100; ms >= 1; --ms) metrics.record_client_report(ms, 0, 0);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.rtt.p50_ms, 50u);  // nearest rank: the 50th of 100
    EXPECT_EQ(r.rtt.p95_ms, 95u);
    EXPECT_EQ(r.rtt.max_ms, 100u);
}

TEST(Metrics, AnUnmeasuredRoundTripIsNotASample) {
    lit::game::Metrics metrics;
    metrics.record_client_report(0, 0, 0);  // no Pong yet on that client
    metrics.record_client_report(80, 0, 0);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.rtt.p50_ms, 80u);
    EXPECT_EQ(r.rtt.max_ms, 80u);
}

TEST(Metrics, PredictionCorrectionsAreSummedWithTheLargest) {
    lit::game::Metrics metrics;
    metrics.record_client_report(40, 2, 7);
    metrics.record_client_report(0, 3, 12);  // corrections count without a round trip
    metrics.record_client_report(50, 0, 0);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.corrections, 5u);
    EXPECT_EQ(r.max_correction, 12u);
}

TEST(Metrics, CountsSpawnsDeathsAndCaptures) {
    lit::game::Metrics metrics;
    metrics.record_spawn();
    metrics.record_spawn();
    metrics.record_deaths(3);
    metrics.record_deaths(0);
    metrics.record_captures(2);
    metrics.record_captures(1);

    const auto r = metrics.report(60, 0);

    EXPECT_EQ(r.spawns, 2u);
    EXPECT_EQ(r.deaths, 3u);
    EXPECT_EQ(r.captures, 3u);
}

TEST(Metrics, AReportStartsTheNextPeriod) {
    lit::game::Metrics metrics;
    metrics.record_tick(500, true);
    metrics.record_snapshot(1000);
    metrics.record_drop();
    metrics.record_resume();
    metrics.record_client_report(90, 4, 20);
    metrics.record_spawn();
    metrics.record_deaths(1);
    metrics.record_captures(1);
    metrics.record_error(::game::v1::ERROR_CODE_KICKED);
    metrics.report(60, 1);

    const auto r = metrics.report(60, 1);

    EXPECT_EQ(r.ticks, 0u);
    EXPECT_EQ(r.tick.max_us, 0u);
    EXPECT_EQ(r.snapshots, 0u);
    EXPECT_EQ(r.drops, 0u);
    EXPECT_EQ(r.resumes, 0u);
    EXPECT_EQ(r.rtt.max_ms, 0u);
    EXPECT_EQ(r.corrections, 0u);
    EXPECT_EQ(r.max_correction, 0u);
    EXPECT_EQ(r.spawns, 0u);
    EXPECT_EQ(r.deaths, 0u);
    EXPECT_EQ(r.captures, 0u);
    EXPECT_TRUE(r.errors.empty());
}

TEST(Metrics, TheLogLineIsOneJsonObject) {
    lit::game::Metrics metrics;
    metrics.record_tick(1200, true);
    metrics.record_snapshot(812);
    metrics.record_join();
    metrics.record_resume();
    metrics.record_client_report(45, 2, 9);
    metrics.record_spawn();
    metrics.record_deaths(1);
    metrics.record_captures(4);
    metrics.record_error(::game::v1::ERROR_CODE_RATE_LIMITED);

    const auto j = nlohmann::json::parse(lit::game::to_json(metrics.report(60, 3)));

    EXPECT_EQ(j.at("period_s"), 60);
    EXPECT_EQ(j.at("ccu"), 3);
    EXPECT_EQ(j.at("ticks"), 1);
    EXPECT_EQ(j.at("tick_us").at("avg"), 1200);
    EXPECT_EQ(j.at("tick_us").at("p99"), 1200);
    EXPECT_EQ(j.at("tick_us").at("max"), 1200);
    EXPECT_EQ(j.at("snapshot_ticks"), 1);
    EXPECT_EQ(j.at("snapshot_tick_us").at("max"), 1200);
    EXPECT_EQ(j.at("snapshots"), 1);
    EXPECT_EQ(j.at("snapshot_bytes_avg"), 812);
    EXPECT_EQ(j.at("drops"), 0);
    EXPECT_EQ(j.at("resyncs"), 0);
    EXPECT_EQ(j.at("closed_behind"), 0);
    EXPECT_EQ(j.at("joins"), 1);
    EXPECT_EQ(j.at("leaves"), 0);
    EXPECT_EQ(j.at("resumes"), 1);
    EXPECT_EQ(j.at("rtt_ms").at("p50"), 45);
    EXPECT_EQ(j.at("rtt_ms").at("p95"), 45);
    EXPECT_EQ(j.at("rtt_ms").at("max"), 45);
    EXPECT_EQ(j.at("corrections").at("count"), 2);
    EXPECT_EQ(j.at("corrections").at("max"), 9);
    EXPECT_EQ(j.at("spawns"), 1);
    EXPECT_EQ(j.at("deaths"), 1);
    EXPECT_EQ(j.at("captures"), 4);
    EXPECT_EQ(j.at("errors").at("RATE_LIMITED"), 1);
}
