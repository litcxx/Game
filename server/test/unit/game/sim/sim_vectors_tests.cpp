// The formulas the client's prediction repeats — a step of movement, a shot's
// velocity along an aim and its flight — pinned as cases in protocol/sim/*.json,
// which the client's checks read too. The server is the reference: these tests
// compare the files with what its functions give now, to the bit. After a change
// to a formula or a case, write them anew:
//   UPDATE_SIM_VECTORS=1 ./build/bin/unit_tests --gtest_filter='SimVectors.*'
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "state/projectile.hpp"
#include "systems/movement_system.hpp"
#include "systems/projectile_system.hpp"

namespace {

using Json = nlohmann::ordered_json;

constexpr double kDt = 1.0 / 60;  // the shipped tick
constexpr double kMap = 10000.0;  // a 100 x 100-cell map, in units

Json movement_vectors() {
    struct Case {
        const char* name;
        double x;
        double y;
        std::int32_t move_x;
        std::int32_t move_y;
        int steps;
        double speed = 300.0;  // move_speed
    };
    const Case cases[] = {
        {"right, one step", 5000, 5000, 1, 0, 1},
        {"diagonal, one step", 5000, 5000, 1, 1, 1},
        {"diagonal for a second", 5000, 5000, -1, 1, 60},
        {"any length is normalized", 5000, 5000, 3, -4, 30},
        {"standing still", 1234.5, 678.25, 0, 0, 10},
        {"stopped at the left edge", 3, 5000, -1, 0, 5},
        {"stopped in the bottom-right corner", 9995, 9998, 1, 1, 10},
        {"along the top edge", 100, 2, 1, -1, 20},
        // From the corner, where a position is small enough to keep the last bit
        // of a step — so the order of the operations shows:
        {"from the corner, diagonal at another speed", 0, 0, 1, 1, 1, 250.0},
        {"from the corner, a slanted direction", 0, 0, 2, 7, 1, 250.0},
        {"from the corner, another slanted direction", 0, 0, 5, 3, 1, 251.0},
    };
    const double max = kMap - 1.0;  // as World clamps: the last unit of the map
    Json out = {{"about",
                 "One step of movement (server: step_position, client: integrate), "
                 "repeated `steps` times. Written by the server's unit tests "
                 "(SimVectors); the client's sim_vectors_check reads it."},
                {"cases", Json::array()}};
    for (const Case& c : cases) {
        lit::game::Position p{c.x, c.y};
        for (int i = 0; i < c.steps; ++i) {
            p = lit::game::step_position(p, c.move_x, c.move_y, c.speed, kDt, max, max);
        }
        out["cases"].push_back({{"name", c.name},
                                {"x", c.x},
                                {"y", c.y},
                                {"move_x", c.move_x},
                                {"move_y", c.move_y},
                                {"speed", c.speed},
                                {"dt", kDt},
                                {"max_x", max},
                                {"max_y", max},
                                {"steps", c.steps},
                                {"after", {{"x", p.x}, {"y", p.y}}}});
    }
    return out;
}

// A shot from (x, y) along the aim, flown tick by tick until it is spent — as
// update_projectiles flies it, hits aside: off the map first, then out of range.
Json shot(const char* name, double x, double y, std::int32_t aim_x, std::int32_t aim_y,
          double speed, double range) {
    Json c = {
        {"name", name},   {"x", x},         {"y", y},    {"aim_x", aim_x},    {"aim_y", aim_y},
        {"speed", speed}, {"range", range}, {"dt", kDt}, {"map_width", kMap}, {"map_height", kMap}};
    const auto v = lit::game::aim_velocity(aim_x, aim_y, speed);
    if (!v) {
        c["velocity"] = nullptr;  // no aim: no shot
        c["path"] = Json::array();
        c["end"] = nullptr;
        return c;
    }
    lit::game::Projectile p;
    p.x = x;
    p.y = y;
    p.vx = v->vx;
    p.vy = v->vy;
    p.remaining = range;
    Json path = Json::array();
    std::string reason;
    while (reason.empty() && path.size() < 10000) {
        const lit::game::FlightStep s = lit::game::flight_step(p, kDt);
        p.x = s.x;
        p.y = s.y;
        p.remaining -= s.length;
        path.push_back(Json::array({s.x, s.y}));
        if (lit::game::off_map(s.x, s.y, kMap, kMap)) {
            reason = "edge";
        } else if (p.remaining <= 0.0) {
            reason = "range";
        }
    }
    c["velocity"] = {{"vx", v->vx}, {"vy", v->vy}};
    c["path"] = path;  // where it is after each tick of flight, the tick it is launched first
    c["end"] = {{"tick", path.size()}, {"reason", reason}};
    return c;
}

Json projectile_vectors() {
    Json out = {{"about",
                 "A shot's velocity along an aim (server: aim_velocity) and its flight "
                 "tick by tick, hits aside (flight_step, off_map), from its launch "
                 "tick until it is spent. Written by the server's unit tests "
                 "(SimVectors); the client's checks read it."},
                {"cases", Json::array()}};
    auto& cases = out["cases"];
    cases.push_back(shot("east", 5000, 5000, 1000, 0, 600, 500));
    cases.push_back(shot("west", 5000, 5000, -1000, 0, 600, 500));
    cases.push_back(shot("north", 5000, 5000, 0, -1000, 600, 500));
    cases.push_back(shot("south", 5000, 5000, 0, 1000, 600, 500));
    cases.push_back(shot("north-east", 5000, 5000, 707, -707, 600, 500));
    cases.push_back(shot("north-west", 5000, 5000, -707, -707, 600, 500));
    cases.push_back(shot("south-east", 5000, 5000, 707, 707, 600, 500));
    cases.push_back(shot("south-west", 5000, 5000, -707, 707, 600, 500));
    cases.push_back(shot("an oblique aim", 4321.5, 6789.25, 812, -584, 600, 500));
    cases.push_back(shot("an aim of any length", 5000, 5000, 30000, 40000, 600, 500));
    // Its speed by Math.hypot is 600, by the sqrt of the squares 599.9999999999999:
    // only the latter is the server's.
    cases.push_back(shot("almost straight up", 5000, 5000, 1, -1000, 600, 500));
    cases.push_back(shot("range not a whole number of steps", 5000, 5000, 1000, 0, 600, 505));
    cases.push_back(shot("another speed and range", 5000, 5000, -300, 950, 450, 300));
    cases.push_back(shot("off the left edge", 25, 5000, -1000, 0, 600, 500));
    cases.push_back(shot("off the right edge", 9985, 5000, 1000, 0, 600, 500));
    cases.push_back(shot("off the top edge", 5000, 15, 0, -1000, 600, 500));
    cases.push_back(shot("off the bottom edge", 5000, 9992, 0, 1000, 600, 500));
    cases.push_back(shot("no aim", 5000, 5000, 0, 0, 600, 500));
    return out;
}

// Compares protocol/sim/<file> with what the server gives, or writes it.
void check_or_write(const std::string& file, const Json& now) {
    const std::string path = std::string(LIT_SIM_VECTORS_DIR) + "/" + file;
    if (std::getenv("UPDATE_SIM_VECTORS") != nullptr) {
        std::ofstream(path) << now.dump(2) << '\n';
        std::cout << "wrote " << path << '\n';
        return;
    }
    std::ifstream in(path);
    ASSERT_TRUE(in) << path << " is missing; UPDATE_SIM_VECTORS=1 writes it";
    EXPECT_EQ(Json::parse(in), now) << "the server's formulas no longer give " << path
                                    << ": if that is meant, write it anew (UPDATE_SIM_VECTORS=1) "
                                       "and change the client to match";
}

}  // namespace

TEST(SimVectors, MovementMatchesTheServer) { check_or_write("movement.json", movement_vectors()); }

TEST(SimVectors, ProjectileFlightMatchesTheServer) {
    check_or_write("projectile.json", projectile_vectors());
}
