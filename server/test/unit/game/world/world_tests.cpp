#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game/mock_client_gateway.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_event.hpp"
#include "utils/ts_queue.hpp"
#include "world/world.hpp"

namespace {

lit::GameConfig test_config() {
    lit::GameConfig c{};
    c.tick_rate = 60;
    c.snapshot_rate = 20;
    c.map_width = 4;
    c.map_height = 4;
    c.move_speed = 300;
    c.max_hp = 100;
    c.attack_range = 120;
    c.attack_cooldown_ticks = 45;
    c.respawn_delay_ticks = 300;
    c.reconnect_grace_ms = 30000;
    c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
    return c;
}

lit::ClientEvent hello_event(std::uint64_t session_id, std::string name) {
    lit::ClientEvent ev;
    ev.session_id = session_id;
    ev.kind = lit::ClientEvent::Kind::Message;
    auto* hello = ev.msg.mutable_hello();
    hello->set_protocol_version(1);
    hello->set_name(std::move(name));
    return ev;
}

lit::ClientEvent disconnect_event(std::uint64_t session_id) {
    lit::ClientEvent ev;
    ev.session_id = session_id;
    ev.kind = lit::ClientEvent::Kind::Disconnected;
    return ev;
}

// All ServerMessages delivered to one session, parsed back from bytes.
std::vector<::game::v1::ServerMessage> messages_to(const lit::test::MockClientGateway& gw,
                                                   std::uint64_t session_id) {
    std::vector<::game::v1::ServerMessage> out;
    for (const auto& s : gw.sent) {
        if (s.session_id != session_id) continue;
        ::game::v1::ServerMessage m;
        if (m.ParseFromArray(s.bytes.data(), static_cast<int>(s.bytes.size())))
            out.push_back(std::move(m));
    }
    return out;
}

}  // namespace

TEST(WorldHello, AssignsPlayerIdAndSendsWelcome) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(42, "tag"));
    world.tick(0.016);

    bool found = false;
    ::game::v1::Welcome welcome;
    for (const auto& m : messages_to(gw, 42))
        if (m.has_welcome()) {
            welcome = m.welcome();
            found = true;
        }

    ASSERT_TRUE(found) << "Hello должен вызвать Welcome для этой сессии";
    EXPECT_GE(welcome.player_id(), 1u);
}

TEST(WorldHello, WelcomeCarriesTickConfigAndFactions) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(42, "tag"));
    world.tick(0.016);

    bool found = false;
    ::game::v1::Welcome welcome;
    for (const auto& m : messages_to(gw, 42))
        if (m.has_welcome()) {
            welcome = m.welcome();
            found = true;
        }
    ASSERT_TRUE(found);

    EXPECT_GE(welcome.server_tick(), 1u);
    EXPECT_EQ(welcome.config().tick_rate(), config.tick_rate);
    EXPECT_EQ(welcome.config().map_width(), config.map_width);
    EXPECT_EQ(welcome.config().map_height(), config.map_height);
    EXPECT_EQ(welcome.config().max_hp(), config.max_hp);
    ASSERT_EQ(welcome.factions_size(), static_cast<int>(config.factions.size()));
    EXPECT_EQ(welcome.factions(0).id(), config.factions[0].id);
    EXPECT_EQ(welcome.factions(0).name(), config.factions[0].name);
    EXPECT_EQ(welcome.factions(0).color(), config.factions[0].color);
}

TEST(WorldHello, SendsNeutralMapStateToJoiner) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(42, "tag"));
    world.tick(0.016);

    bool found = false;
    ::game::v1::MapState map;
    for (const auto& m : messages_to(gw, 42))
        if (m.has_map_state()) {
            map = m.map_state();
            found = true;
        }
    ASSERT_TRUE(found);
    EXPECT_EQ(map.owners().size(), static_cast<std::size_t>(config.map_width) * config.map_height);
    for (char b : map.owners()) EXPECT_EQ(b, 0);
}

TEST(WorldHello, SendsFullRosterIncludingSelf) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(42, "tag"));
    world.tick(0.016);

    bool found = false;
    ::game::v1::Roster roster;
    for (const auto& m : messages_to(gw, 42))
        if (m.has_roster()) {
            roster = m.roster();
            found = true;
        }
    ASSERT_TRUE(found);
    ASSERT_EQ(roster.upsert_size(), 1);
    EXPECT_EQ(roster.upsert(0).name(), "tag");
    EXPECT_GE(roster.upsert(0).id(), 1u);
}

TEST(WorldRoster, JoinNotifiesExistingPlayers) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "alice"));
    world.tick(0.016);
    incoming.push(hello_event(2, "bob"));
    world.tick(0.016);

    // alice (session 1) must be told about bob via a Roster upsert.
    bool bob_notified = false;
    for (const auto& m : messages_to(gw, 1))
        if (m.has_roster())
            for (const auto& info : m.roster().upsert())
                if (info.name() == "bob") bob_notified = true;

    EXPECT_TRUE(bob_notified);
}

TEST(WorldRoster, DisconnectRemovesPlayerAndNotifiesOthers) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "alice"));
    world.tick(0.016);
    incoming.push(hello_event(2, "bob"));
    world.tick(0.016);

    // Capture bob's assigned player_id from the upsert alice received.
    std::uint32_t bob_id = 0;
    for (const auto& m : messages_to(gw, 1))
        if (m.has_roster())
            for (const auto& info : m.roster().upsert())
                if (info.name() == "bob") bob_id = info.id();
    ASSERT_GT(bob_id, 0u);

    incoming.push(disconnect_event(2));
    world.tick(0.016);

    // alice must be told bob was removed.
    bool bob_removed = false;
    for (const auto& m : messages_to(gw, 1))
        if (m.has_roster())
            for (auto removed_id : m.roster().removed())
                if (removed_id == bob_id) bob_removed = true;

    EXPECT_TRUE(bob_removed);
}

namespace {

[[maybe_unused]] lit::ClientEvent spawn_event(std::uint64_t session_id, std::uint32_t cell,
                                              std::uint32_t faction_id) {
    lit::ClientEvent ev;
    ev.session_id = session_id;
    ev.kind = lit::ClientEvent::Kind::Message;
    auto* spawn = ev.msg.mutable_spawn();
    spawn->set_cell(cell);
    spawn->set_faction_id(faction_id);
    return ev;
}

[[maybe_unused]] lit::ClientEvent input_event(std::uint64_t session_id, std::int32_t move_x,
                                              std::int32_t move_y, std::uint32_t seq) {
    lit::ClientEvent ev;
    ev.session_id = session_id;
    ev.kind = lit::ClientEvent::Kind::Message;
    auto* frame = ev.msg.mutable_input()->add_frames();
    frame->set_seq(seq);
    frame->set_move_x(move_x);
    frame->set_move_y(move_y);
    return ev;
}

// The last Snapshot delivered to a session (snapshots are periodic).
std::optional<::game::v1::Snapshot> last_snapshot_to(const lit::test::MockClientGateway& gw,
                                                    std::uint64_t session_id) {
    std::optional<::game::v1::Snapshot> snap;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot()) snap = m.snapshot();
    return snap;
}

void run_ticks(lit::game::World& world, int n, double dt) {
    for (int i = 0; i < n; ++i) world.tick(dt);
}

}  // namespace

TEST(WorldSnapshot, SendsSnapshotWithSelfStateToConnectedPlayer) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    run_ticks(world, 3, 0.016);  // tick_rate / snapshot_rate = 3 -> one snapshot

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_NOT_SPAWNED);
    EXPECT_EQ(snap->players_size(), 0);  // nobody has spawned yet
}

TEST(WorldSpawn, PlacesPlayerAliveAtCellCenterWithFullHp) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // 4-wide map: col 1, row 1
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_ALIVE);
    ASSERT_EQ(snap->players_size(), 1);
    const auto& ps = snap->players(0);
    EXPECT_EQ(ps.x(), 150u);  // 1 * 100 + 50
    EXPECT_EQ(ps.y(), 150u);
    EXPECT_EQ(ps.hp(), config.max_hp);
}

TEST(WorldSpawn, RejectsOutOfBoundsCell) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/9999, /*faction=*/1));
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_NOT_SPAWNED);
    EXPECT_EQ(snap->players_size(), 0);
}

TEST(WorldSpawn, RejectsUnknownFaction) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/99));  // not in config
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_NOT_SPAWNED);
}

TEST(WorldSpawn, NotifiesOthersOfChosenFaction) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/2));
    world.tick(0.05);

    bool told = false;
    for (const auto& m : messages_to(gw, 2))
        if (m.has_roster())
            for (const auto& info : m.roster().upsert())
                if (info.name() == "a" && info.faction_id() == 2) told = true;
    EXPECT_TRUE(told);
}

TEST(WorldMovement, InputMovesAlivePlayer) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // center (150,150)
    incoming.push(input_event(7, /*move_x=*/1, /*move_y=*/0, /*seq=*/1));
    run_ticks(world, 3, 0.1);  // move_speed 300 * 0.1 = 30 units/tick

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->players_size(), 1);
    EXPECT_GT(snap->players(0).x(), 150u);
    EXPECT_EQ(snap->players(0).y(), 150u);
    EXPECT_EQ(snap->you().last_input_seq(), 1u);
}

TEST(WorldMovement, StandsStillWithoutInput) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));
    run_ticks(world, 3, 0.1);

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->players_size(), 1);
    EXPECT_EQ(snap->players(0).x(), 150u);
    EXPECT_EQ(snap->players(0).y(), 150u);
}

TEST(WorldMovement, IgnoresInputWhenNotSpawned) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(input_event(7, /*move_x=*/1, /*move_y=*/0, /*seq=*/1));
    run_ticks(world, 3, 0.1);

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_NOT_SPAWNED);
    EXPECT_EQ(snap->players_size(), 0);
}
