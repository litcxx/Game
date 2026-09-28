#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
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
    c.respawn_delay_ticks = 5;
    c.reconnect_grace_ms = 30000;
    c.capture_ticks = 5;  // small so capture tests flip quickly
    c.player_radius = 16;
    c.vision_radius = 1000;  // sees the whole 4x4 map: fog of war hides nothing here
                             //   (the WorldFog suite uses fog_config())
    c.factions = {{1, "Red", 0xFF0000}, {2, "Blue", 0x0000FF}};
    // Bar order: abilities[0] is the default (ability 0 in input) — melee.
    // Melee: cooldown 10 -> one swing in a short test window; 40 dmg -> 3 hits kill.
    // Block: its own cooldown (10, like melee), active for 9 ticks.
    c.abilities = {
        {1, lit::AbilityKind::Melee, "Strike", 10, 40, 120, 0, 0, 0},
        {2, lit::AbilityKind::Projectile, "Shot", 30, 25, 250, 600, 8, 0},
        {3, lit::AbilityKind::Block, "Guard", 10, 0, 0, 0, 0, 9},
    };
    return c;
}

// The melee ability of test_config() (bar slot 1).
lit::AbilityConfig& melee(lit::GameConfig& c) { return c.abilities[0]; }

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

TEST(WorldHello, WelcomeCarriesAbilitiesAndPlayerRadius) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(42, "tag"));
    world.tick(0.016);

    std::optional<::game::v1::Welcome> welcome;
    for (const auto& m : messages_to(gw, 42))
        if (m.has_welcome()) welcome = m.welcome();
    ASSERT_TRUE(welcome.has_value());

    EXPECT_EQ(welcome->config().player_radius(), 16u);
    ASSERT_EQ(welcome->abilities_size(), 3);  // bar order kept
    const auto& strike = welcome->abilities(0);
    EXPECT_EQ(strike.id(), 1u);
    EXPECT_EQ(strike.kind(), ::game::v1::ABILITY_KIND_MELEE);
    EXPECT_EQ(strike.name(), "Strike");
    EXPECT_EQ(strike.cooldown_ticks(), 10u);
    EXPECT_EQ(strike.damage(), 40u);
    EXPECT_EQ(strike.range(), 120u);
    const auto& shot = welcome->abilities(1);
    EXPECT_EQ(shot.id(), 2u);
    EXPECT_EQ(shot.kind(), ::game::v1::ABILITY_KIND_PROJECTILE);
    EXPECT_EQ(shot.name(), "Shot");
    EXPECT_EQ(shot.cooldown_ticks(), 30u);
    EXPECT_EQ(shot.damage(), 25u);
    EXPECT_EQ(shot.range(), 250u);
    EXPECT_EQ(shot.projectile_speed(), 600u);
    EXPECT_EQ(shot.projectile_radius(), 8u);
    const auto& guard = welcome->abilities(2);
    EXPECT_EQ(guard.id(), 3u);
    EXPECT_EQ(guard.kind(), ::game::v1::ABILITY_KIND_BLOCK);
    EXPECT_EQ(guard.name(), "Guard");
    EXPECT_EQ(guard.cooldown_ticks(), 10u);
    EXPECT_EQ(guard.duration_ticks(), 9u);
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

lit::ClientEvent input_event(std::uint64_t session_id, std::int32_t move_x, std::int32_t move_y,
                             std::uint32_t seq, bool capturing = false, bool attack = false) {
    lit::ClientEvent ev;
    ev.session_id = session_id;
    ev.kind = lit::ClientEvent::Kind::Message;
    auto* frame = ev.msg.mutable_input()->add_frames();
    frame->set_seq(seq);
    frame->set_move_x(move_x);
    frame->set_move_y(move_y);
    frame->set_capturing(capturing);
    frame->set_attack(attack);
    return ev;
}

// The last Snapshot delivered to a session (snapshots are periodic).
// A held attack with the given ability (and aim, for projectiles).
lit::ClientEvent attack_event(std::uint64_t session_id, std::uint32_t seq, std::uint32_t ability,
                              std::int32_t aim_x = 0, std::int32_t aim_y = 0) {
    auto ev = input_event(session_id, 0, 0, seq, /*capturing=*/false, /*attack=*/true);
    auto* frame = ev.msg.mutable_input()->mutable_frames(0);
    frame->set_ability(ability);
    frame->set_aim_x(aim_x);
    frame->set_aim_y(aim_y);
    return ev;
}

std::optional<::game::v1::Snapshot> last_snapshot_to(const lit::test::MockClientGateway& gw,
                                                     std::uint64_t session_id) {
    std::optional<::game::v1::Snapshot> snap;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot()) snap = m.snapshot();
    return snap;
}

// The last CellUpdate seen for a given cell across snapshots to a session.
std::optional<::game::v1::CellUpdate> last_cell_update(const lit::test::MockClientGateway& gw,
                                                       std::uint64_t session_id,
                                                       std::uint32_t index) {
    std::optional<::game::v1::CellUpdate> out;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& c : m.snapshot().cells())
                if (c.index() == index) out = c;
    return out;
}

// The player's PlayerState in the last snapshot to a session (if present).
std::optional<::game::v1::PlayerState> player_state_in(const lit::test::MockClientGateway& gw,
                                                       std::uint64_t session_id, std::uint32_t id) {
    std::optional<::game::v1::PlayerState> out;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& p : m.snapshot().players())
                if (p.id() == id) out = p;
    return out;
}

int count_hits(const lit::test::MockClientGateway& gw, std::uint64_t session_id) {
    int n = 0;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& e : m.snapshot().events())
                if (e.has_hit()) ++n;
    return n;
}

int count_deaths(const lit::test::MockClientGateway& gw, std::uint64_t session_id) {
    int n = 0;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& e : m.snapshot().events())
                if (e.has_death()) ++n;
    return n;
}

// Hits on `id` seen by `session_id`, split into blocked and landed.
std::pair<int, int> blocked_and_landed_hits(const lit::test::MockClientGateway& gw,
                                            std::uint64_t session_id, std::uint32_t id) {
    int blocked = 0;
    int landed = 0;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& e : m.snapshot().events())
                if (e.has_hit() && e.hit().target_id() == id)
                    (e.hit().blocked() ? blocked : landed)++;
    return {blocked, landed};
}

// AbilityEvents (player id, ability id) seen by `session_id`, in order.
std::vector<std::pair<std::uint32_t, std::uint32_t>> ability_events(
    const lit::test::MockClientGateway& gw, std::uint64_t session_id) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot())
            for (const auto& e : m.snapshot().events())
                if (e.has_ability())
                    out.emplace_back(e.ability().player_id(), e.ability().ability_id());
    return out;
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

TEST(WorldCapture, HoldingCaptureFlipsCellOwner) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();  // capture_ticks = 5
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // center (150,150) -> cell 5
    incoming.push(input_event(7, 0, 0, /*seq=*/1, /*capturing=*/true));
    run_ticks(world, 6, 0.016);  // 5 ticks * 20% -> flips by tick 5

    auto cu = last_cell_update(gw, 7, /*index=*/5);
    ASSERT_TRUE(cu.has_value());
    EXPECT_EQ(cu->owner(), 1u);  // cell now owned by faction 1
}

TEST(WorldCapture, ReleasingCaptureResetsProgress) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));
    incoming.push(input_event(7, 0, 0, /*seq=*/1, /*capturing=*/true));
    run_ticks(world, 3, 0.016);  // partial (~60%), not yet captured
    incoming.push(input_event(7, 0, 0, /*seq=*/2, /*capturing=*/false));  // release
    run_ticks(world, 3, 0.016);

    auto cu = last_cell_update(gw, 7, /*index=*/5);
    ASSERT_TRUE(cu.has_value());
    EXPECT_EQ(cu->owner(), 0u);             // never captured
    EXPECT_EQ(cu->capture_progress(), 0u);  // progress reset
}

TEST(WorldCapture, NoCaptureWithoutHoldingKey) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // spawned, but not capturing
    run_ticks(world, 6, 0.016);

    // The cell's state arrives once, when spawning reveals it — still untouched.
    auto cu = last_cell_update(gw, 7, /*index=*/5);
    ASSERT_TRUE(cu.has_value());
    EXPECT_EQ(cu->owner(), 0u);
    EXPECT_EQ(cu->capture_faction(), 0u);
    EXPECT_EQ(cu->capture_progress(), 0u);
}

// --- M3: combat (area attack) / death / respawn ---------------------------
// Attack is a key held in the InputFrame: while held and off cooldown the
// attacker hits every enemy within attack_range around it. Ids 1,2,3 in hello
// order. 4-wide test map cells: 5->(150,150), 6->(250,150), 4->(50,150) (each
// 100 from cell 5, within range 120), 15->(350,350) (far).

TEST(WorldCombat, AttackHitsEnemyInRange) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();  // cooldown 10 -> a single swing in a short window
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, /*session=*/2, /*id=*/2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), config.max_hp - melee(config).damage);  // one swing
    EXPECT_GE(count_hits(gw, 2), 1);
}

TEST(WorldCombat, AttackHitsAllEnemiesInArea) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(hello_event(3, "c"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));  // attacker (150,150)
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));  // enemy at (250,150)
    incoming.push(spawn_event(3, /*cell=*/4, /*faction=*/2));  // enemy at (50,150)
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));
    run_ticks(world, 3, 0.016);

    auto b = player_state_in(gw, 2, 2);
    auto c = player_state_in(gw, 3, 3);
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(b->hp(), config.max_hp - melee(config).damage);  // both struck by one swing
    EXPECT_EQ(c->hp(), config.max_hp - melee(config).damage);
    EXPECT_GE(count_hits(gw, 1), 2);
}

TEST(WorldCombat, NoAttackWithoutKey) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));  // enemy in range
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/false));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, /*session=*/2, /*id=*/2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), config.max_hp);  // never attacked -> unharmed
    EXPECT_EQ(count_hits(gw, 2), 0);
}

TEST(WorldCombat, NoFriendlyFire) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/1));  // same faction
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, /*session=*/2, /*id=*/2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), config.max_hp);  // ally unharmed
    EXPECT_EQ(count_hits(gw, 2), 0);
}

TEST(WorldCombat, OutOfRangeNoDamage) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));   // (150,150)
    incoming.push(spawn_event(2, /*cell=*/15, /*faction=*/2));  // (350,350) -> far
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, /*session=*/2, /*id=*/2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), config.max_hp);  // out of range, unharmed
}

TEST(WorldCombat, CooldownLimitsSwings) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    melee(config).cooldown_ticks = 3;  // only one swing fits in the 3-tick window
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));  // held
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, /*session=*/2, /*id=*/2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), config.max_hp - melee(config).damage);  // exactly one swing
    EXPECT_EQ(count_hits(gw, 2), 1);
}

TEST(WorldCombat, KillsAndSetsDead) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    melee(config).cooldown_ticks = 1;  // swing every tick -> 3 swings kill 100 hp
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));  // held
    run_ticks(world, 6, 0.016);

    auto snap = last_snapshot_to(gw, 2);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_DEAD);
    EXPECT_GE(count_deaths(gw, 2), 1);

    auto body = player_state_in(gw, 2, 2);  // body stays where it died
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->hp(), 0u);
}

TEST(WorldCombat, RespawnAfterDelay) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    melee(config).cooldown_ticks = 1;
    config.respawn_delay_ticks = 5;
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/false, /*attack=*/true));  // held
    run_ticks(world, 3, 0.016);  // b dies (~tick 3), respawn_tick = 3 + 5
    incoming.push(
        input_event(1, 0, 0, /*seq=*/2, /*capturing=*/false, /*attack=*/false));  // a stops
    run_ticks(world, 6, 0.016);                                 // wait past respawn_tick
    incoming.push(spawn_event(2, /*cell=*/15, /*faction=*/2));  // respawn far from a
    run_ticks(world, 3, 0.016);

    auto snap = last_snapshot_to(gw, 2);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_ALIVE);
    auto self = player_state_in(gw, 2, 2);
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(self->hp(), config.max_hp);  // full hp on respawn
}

// --- Fixed-step input model -------------------------------------------------
TEST(WorldInput, FixedStepMovementIsDeterministic) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));              // center (150,150)
    incoming.push(input_event(7, /*move_x=*/1, /*move_y=*/0, /*seq=*/1));  // then repeat-last
    const double dt = 1.0 / config.tick_rate;
    run_ticks(world, 6, dt);  // tick1 consumes cmd, ticks 2..6 repeat it

    auto snap = last_snapshot_to(gw, 7);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->players_size(), 1);
    const double expected = 150.0 + 6 * config.move_speed * dt;  // 6 ticks of +speed*dt
    EXPECT_NEAR(static_cast<double>(snap->players(0).x()), expected, 1.0);
    EXPECT_EQ(snap->players(0).y(), 150u);
}

TEST(WorldInput, ConsumesOneCommandPerTick) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    config.snapshot_rate = config.tick_rate;  // one snapshot per tick, to read per-tick acks
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));
    incoming.push(input_event(7, 0, 0, /*seq=*/1));
    incoming.push(input_event(7, 0, 0, /*seq=*/2));
    const double dt = 1.0 / config.tick_rate;

    run_ticks(world, 1, dt);
    EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 1u);  // consumed seq 1
    run_ticks(world, 1, dt);
    EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 2u);  // consumed seq 2
    run_ticks(world, 1, dt);
    EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 2u);  // empty queue -> repeat
}

TEST(WorldInput, EnqueuesAfterRespawnClear) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    config.snapshot_rate = config.tick_rate;
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(7, "p"));
    incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // clears any queued input
    incoming.push(input_event(7, 1, 0, /*seq=*/10));           // seq keeps climbing
    const double dt = 1.0 / config.tick_rate;
    run_ticks(world, 1, dt);
    EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 10u);  // still enqueued & consumed
}

// --- Fixed-timestep accumulator ---------------------------------------------
TEST(FixedStep, RunsOneStepPerInterval) {
    double acc = 0.0;
    EXPECT_EQ(lit::game::fixed_steps(acc, 1.0 / 60, 1.0 / 60, 0.25), 1);
    EXPECT_NEAR(acc, 0.0, 1e-9);
}

TEST(FixedStep, AccumulatesRemainder) {
    double acc = 0.0;
    const double f = 1.0 / 60;
    EXPECT_EQ(lit::game::fixed_steps(acc, 2.5 * f, f, 0.25), 2);
    EXPECT_NEAR(acc, 0.5 * f, 1e-9);
}

TEST(FixedStep, ClampsSpiralOfDeath) {
    double acc = 0.0;
    const double f = 1.0 / 60;
    EXPECT_EQ(lit::game::fixed_steps(acc, 10.0, f, 0.25), 15);  // 0.25s / (1/60) = 15
    EXPECT_LT(acc, f);
}

// --- Abilities: selection by id, shared cooldown ---------------------------
// InputFrame.ability picks the ability (0 = the first); using one starts the
// shared cooldown with that ability's length, reported in SelfState.

TEST(WorldAbility, UsesTheSelectedAbility) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    config.abilities.push_back({4, lit::AbilityKind::Melee, "Heavy", 20, 70, 120, 0, 0, 0});
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/4));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 30u);  // 100 - 70: the Heavy strike, not the default 40
}

TEST(WorldAbility, SelfStateReportsCooldownLength) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/1));
    run_ticks(world, 3, 0.016);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().attack_cooldown_ticks(), 10u);  // Strike's cooldown
    EXPECT_EQ(snap->you().attack_ready_tick(), 11u);      // swung on tick 1
}

TEST(WorldAbility, UnknownAbilityDoesNothing) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/99));
    run_ticks(world, 3, 0.016);

    auto victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 100u);
    EXPECT_EQ(count_hits(gw, 2), 0);
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().attack_ready_tick(), 0u);  // cooldown untouched
}

// --- Ranged attack: projectiles --------------------------------------------
// test_config() ability 2 "Shot": cooldown 30, 25 dmg, range 250, 600 units/s
// (9.6 units per 0.016 s tick), radius 8; bodies are 16. Cells: 4 -> (50,150),
// 5 -> (150,150), 6 -> (250,150).

TEST(WorldRanged, FiresTowardTheAim) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    run_ticks(world, 3, 0.016);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->projectiles_size(), 1);
    const auto& p = snap->projectiles(0);
    EXPECT_EQ(p.faction_id(), 1u);
    EXPECT_EQ(p.vx(), 600);
    EXPECT_EQ(p.vy(), 0);
    EXPECT_EQ(p.x(), 78u);  // launched from x = 50 on tick 1, 9.6 per tick for 3 ticks
    EXPECT_EQ(p.y(), 150u);
}

TEST(WorldRanged, HitsAnEnemyForTheRangedDamage) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    run_ticks(world, 30, 0.016);  // contact near tick 19; the next shot is due on tick 31

    auto victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 75u);
    EXPECT_EQ(count_hits(gw, 2), 1);
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->projectiles_size(), 0);  // spent on the hit
}

TEST(WorldRanged, NoAimNoShot) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/0, /*aim_y=*/0));
    run_ticks(world, 3, 0.016);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->projectiles_size(), 0);
    EXPECT_EQ(snap->you().attack_ready_tick(), 0u);  // cooldown untouched
}

TEST(WorldRanged, DodgedProjectileMisses) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    incoming.push(input_event(2, 0, 1, /*seq=*/1));  // the target runs out of the line
    run_ticks(world, 30, 0.016);

    auto victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 100u);
    EXPECT_EQ(count_hits(gw, 2), 0);
}

TEST(WorldRanged, SharedCooldownBlocksOtherAbilities) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));  // within melee range
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/0, /*aim_y=*/-1000));
    incoming.push(attack_event(1, /*seq=*/2, /*ability=*/1));  // then hold melee
    run_ticks(world, 12, 0.016);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().attack_cooldown_ticks(), 30u);  // the shot's cooldown
    EXPECT_EQ(snap->you().attack_ready_tick(), 31u);
    auto victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 100u);  // melee is held but must wait for the shared cooldown

    run_ticks(world, 21, 0.016);  // through tick 33: melee swings on tick 31
    victim = player_state_in(gw, 2, 2);
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(victim->hp(), 60u);
}

// --- Block: its own cooldown, no damage while it lasts ----------------------
// test_config() ability 3 "Guard": cooldown 10, lasts 9 ticks. Melee (1):
// cooldown 10, 40 dmg. Cells: 4 -> (50,150), 5 -> (150,150), 6 -> (250,150).

TEST(WorldBlock, BlockPressedWithASwingStopsIt) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "atk"));
    incoming.push(hello_event(2, "def"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));  // within melee range
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/3));  // block on tick 1...
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/1));  // ...the swing on tick 1 too
    run_ticks(world, 3, 0.016);

    auto def = player_state_in(gw, 2, 2);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->hp(), 100u);
    EXPECT_EQ(blocked_and_landed_hits(gw, 2, 2), std::make_pair(1, 0));
}

TEST(WorldBlock, BlockExpires) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "atk"));
    incoming.push(hello_event(2, "def"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/3));  // block once: ticks 1..9
    incoming.push(input_event(2, 0, 0, /*seq=*/2));            // then let go
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/1));  // swings on ticks 1 and 11
    run_ticks(world, 12, 0.016);

    auto def = player_state_in(gw, 2, 2);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->hp(), 60u);  // the second swing landed
    EXPECT_EQ(blocked_and_landed_hits(gw, 2, 2), std::make_pair(1, 1));
}

TEST(WorldBlock, BlockStopsAProjectile) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "shooter"));
    incoming.push(hello_event(2, "def"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    // The shot arrives on tick 19; block on tick 15 (active 15..23), idle before.
    for (std::uint32_t seq = 1; seq <= 14; ++seq) incoming.push(input_event(2, 0, 0, seq));
    incoming.push(attack_event(2, /*seq=*/15, /*ability=*/3));
    incoming.push(input_event(2, 0, 0, /*seq=*/16));
    run_ticks(world, 30, 0.016);

    auto def = player_state_in(gw, 2, 2);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->hp(), 100u);
    EXPECT_EQ(blocked_and_landed_hits(gw, 2, 2), std::make_pair(1, 0));
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->projectiles_size(), 0);  // spent on the block
}

TEST(WorldBlock, BlockHasItsOwnCooldown) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/6, /*faction=*/2));
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/3));  // block on tick 1
    incoming.push(attack_event(1, /*seq=*/2, /*ability=*/1));  // switch: swing on tick 2
    run_ticks(world, 3, 0.016);

    auto enemy = player_state_in(gw, 2, 2);
    ASSERT_TRUE(enemy.has_value());
    EXPECT_EQ(enemy->hp(), 60u);  // the block did not delay the swing
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().block_ready_tick(), 11u);  // 1 + 10
    EXPECT_EQ(snap->you().block_cooldown_ticks(), 10u);
    EXPECT_EQ(snap->you().attack_ready_tick(), 12u);  // 2 + 10
    EXPECT_EQ(snap->you().attack_cooldown_ticks(), 10u);
}

TEST(WorldAbility, EveryUseIsAnnounced) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = test_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/5, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/15, /*faction=*/1));  // far away, an ally
    incoming.push(attack_event(1, /*seq=*/1, /*ability=*/1));   // a swing that hits nobody
    incoming.push(attack_event(1, /*seq=*/2, /*ability=*/3));   // a block
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    run_ticks(world, 3, 0.016);

    auto seen = ability_events(gw, 1);
    std::sort(seen.begin(), seen.end());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> expected{{1, 1}, {1, 3}, {2, 2}};
    EXPECT_EQ(seen, expected);
}

// --- Fog of war --------------------------------------------------------------
// fog_config(): a 10x10 map where sight reaches 2 cells (200 units), so players
// a few cells apart don't see each other. Cell i is (col, row) = (i % 10, i / 10)
// with its centre at (col * 100 + 50, row * 100 + 50). A player sees the cells
// whose centres are within 200 of it; others are visible when their cell is.

namespace {

lit::GameConfig fog_config() {
    auto c = test_config();
    c.map_width = 10;
    c.map_height = 10;
    c.vision_radius = 200;
    return c;
}

std::vector<::game::v1::Snapshot> snapshots_to(const lit::test::MockClientGateway& gw,
                                               std::uint64_t session_id) {
    std::vector<::game::v1::Snapshot> out;
    for (const auto& m : messages_to(gw, session_id))
        if (m.has_snapshot()) out.push_back(m.snapshot());
    return out;
}

bool lists_player(const ::game::v1::Snapshot& snap, std::uint32_t id) {
    return std::ranges::any_of(snap.players(), [id](const auto& p) { return p.id() == id; });
}

bool contains(const google::protobuf::RepeatedField<std::uint32_t>& cells, std::uint32_t index) {
    return std::ranges::find(cells, index) != cells.end();
}

// Every cell index revealed (or hidden) across all snapshots to a session.
std::vector<std::uint32_t> all_revealed(const lit::test::MockClientGateway& gw,
                                        std::uint64_t session_id) {
    std::vector<std::uint32_t> out;
    for (const auto& s : snapshots_to(gw, session_id))
        out.insert(out.end(), s.revealed().begin(), s.revealed().end());
    return out;
}

std::vector<std::uint32_t> all_hidden(const lit::test::MockClientGateway& gw,
                                      std::uint64_t session_id) {
    std::vector<std::uint32_t> out;
    for (const auto& s : snapshots_to(gw, session_id))
        out.insert(out.end(), s.hidden().begin(), s.hidden().end());
    return out;
}

bool has(const std::vector<std::uint32_t>& cells, std::uint32_t index) {
    return std::ranges::find(cells, index) != cells.end();
}

}  // namespace

TEST(WorldFog, AnEnemyOutOfSightIsNotSent) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/99, /*faction=*/2));  // the far corner
    run_ticks(world, 3, 0.05);

    auto seen_by_a = last_snapshot_to(gw, 1);
    ASSERT_TRUE(seen_by_a.has_value());
    EXPECT_TRUE(lists_player(*seen_by_a, 1));  // itself
    EXPECT_FALSE(lists_player(*seen_by_a, 2));
    auto seen_by_b = last_snapshot_to(gw, 2);
    ASSERT_TRUE(seen_by_b.has_value());
    EXPECT_FALSE(lists_player(*seen_by_b, 1));
}

TEST(WorldFog, AnEnemyInSightIsSent) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));  // (50,50)
    incoming.push(spawn_event(2, /*cell=*/2, /*faction=*/2));  // (250,50): its cell is 200 away
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(lists_player(*snap, 2));
}

TEST(WorldFog, AlliesShareTheirSight) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "enemy"));
    incoming.push(hello_event(3, "ally"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/19, /*faction=*/2));  // far from a, next to the ally
    incoming.push(spawn_event(3, /*cell=*/9, /*faction=*/1));
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(lists_player(*snap, 3));  // the ally
    EXPECT_TRUE(lists_player(*snap, 2));  // the enemy, through the ally's eyes
}

TEST(WorldFog, AnEnemyLeavingSightDisappears) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/2, /*faction=*/2));
    incoming.push(input_event(2, /*move_x=*/1, /*move_y=*/0, /*seq=*/1));  // 15 units a tick
    run_ticks(world, 3, 0.05);                                             // b at x = 295
    auto before = last_snapshot_to(gw, 1);
    ASSERT_TRUE(before.has_value());
    EXPECT_TRUE(lists_player(*before, 2));

    run_ticks(world, 3, 0.05);  // b at x = 340: cell 3, out of sight
    auto after = last_snapshot_to(gw, 1);
    ASSERT_TRUE(after.has_value());
    EXPECT_FALSE(lists_player(*after, 2));
}

TEST(WorldFog, ADeadPlayerGivesNoSightButSeesItsBody) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    melee(config).cooldown_ticks = 1;  // three swings on ticks 1..3 kill
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "victim"));
    incoming.push(hello_event(2, "killer"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/1, /*faction=*/2));  // right next to it
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/1));
    run_ticks(world, 6, 0.016);

    // Tick 3: the snapshot reporting the death still sees — who struck it, too.
    EXPECT_EQ(count_deaths(gw, 1), 1);
    // Tick 6: a body sees nothing.
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_DEAD);
    EXPECT_TRUE(lists_player(*snap, 1));  // its own body, always
    EXPECT_FALSE(lists_player(*snap, 2));
}

TEST(WorldFog, SpawningRevealsTheCellsAroundWithTheirState) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/55, /*faction=*/1));  // centre of cell (5,5)
    run_ticks(world, 3, 0.05);

    auto first = last_snapshot_to(gw, 1);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->revealed_size(), 13);  // the disc of cells within 2 cells
    EXPECT_TRUE(contains(first->revealed(), 55));
    EXPECT_TRUE(contains(first->revealed(), 75));  // 2 cells below
    EXPECT_FALSE(contains(first->revealed(), 76));
    EXPECT_EQ(first->cells_size(), 13);  // each revealed cell's current state
    EXPECT_EQ(first->hidden_size(), 0);

    run_ticks(world, 3, 0.05);  // standing still: nothing new
    auto second = last_snapshot_to(gw, 1);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->revealed_size(), 0);
    EXPECT_EQ(second->cells_size(), 0);
}

TEST(WorldFog, MovingRevealsAheadAndHidesBehind) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));  // (50,50)
    incoming.push(input_event(1, 0, 0, /*seq=*/1));
    run_ticks(world, 3, 0.05);  // sees cell 20 = (0,2), 200 away
    ASSERT_TRUE(has(all_revealed(gw, 1), 20));
    ASSERT_FALSE(has(all_revealed(gw, 1), 3));

    incoming.push(input_event(1, /*move_x=*/1, /*move_y=*/0, /*seq=*/2));
    run_ticks(world, 12, 0.05);  // x = 230

    EXPECT_TRUE(has(all_hidden(gw, 1), 20));   // now 269 away
    EXPECT_TRUE(has(all_revealed(gw, 1), 3));  // cell (3,0), now 120 away
}

TEST(WorldFog, ChangesInTheFogArriveOnlyOnceSeen) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();  // capture_ticks = 5
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "enemy"));
    incoming.push(hello_event(3, "ally"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/9, /*faction=*/2));  // far away
    incoming.push(input_event(2, 0, 0, /*seq=*/1, /*capturing=*/true));
    run_ticks(world, 6, 0.016);  // the enemy takes cell 9 unseen

    EXPECT_FALSE(last_cell_update(gw, 1, /*index=*/9).has_value());

    incoming.push(spawn_event(3, /*cell=*/7, /*faction=*/1));  // an ally comes to look
    run_ticks(world, 3, 0.016);

    auto cu = last_cell_update(gw, 1, /*index=*/9);
    ASSERT_TRUE(cu.has_value());
    EXPECT_EQ(cu->owner(), 2u);  // the current state, once it is seen again
}

TEST(WorldFog, OwnedCellsKeepWatch) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/true));
    run_ticks(world, 6, 0.05);  // cell 0 is faction 1's
    incoming.push(input_event(1, /*move_x=*/1, /*move_y=*/0, /*seq=*/2));
    run_ticks(world, 21, 0.05);                                 // a walks off to x = 365
    incoming.push(spawn_event(2, /*cell=*/20, /*faction=*/2));  // 200 from cell 0's centre
    run_ticks(world, 3, 0.05);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(lists_player(*snap, 2));  // seen from the owned cell, not from a
}

TEST(WorldFog, RespawningRevealsTheNewSpot) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    melee(config).cooldown_ticks = 1;
    config.respawn_delay_ticks = 5;
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "killer"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/1, /*faction=*/2));
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/1));  // a dies on tick 3
    run_ticks(world, 3, 0.016);
    incoming.push(input_event(2, 0, 0, /*seq=*/2));
    run_ticks(world, 6, 0.016);
    incoming.push(spawn_event(1, /*cell=*/99, /*faction=*/1));  // the far corner
    run_ticks(world, 3, 0.016);

    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->you().life(), ::game::v1::LIFE_STATE_ALIVE);
    EXPECT_TRUE(contains(snap->revealed(), 99));
    EXPECT_TRUE(contains(snap->revealed(), 97));
    EXPECT_FALSE(lists_player(*snap, 2));  // the killer stayed by the old spot
}

TEST(WorldFog, AShotFromTheFogLandsButTellsNothing) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    config.abilities[1].range = 500;  // the Shot outranges sight
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "victim"));
    incoming.push(hello_event(2, "shooter"));
    incoming.push(spawn_event(1, /*cell=*/4, /*faction=*/1));  // (450,50)
    incoming.push(spawn_event(2, /*cell=*/0, /*faction=*/2));  // (50,50): 400 away
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/2, /*aim_x=*/1000, /*aim_y=*/0));
    run_ticks(world, 21, 0.05);  // lands on tick 13; the next shot is due on tick 31

    auto self = player_state_in(gw, 1, 1);
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(self->hp(), 75u);  // the damage is dealt
    auto snap = last_snapshot_to(gw, 1);
    ASSERT_TRUE(snap.has_value());
    EXPECT_FALSE(lists_player(*snap, 2));
    EXPECT_EQ(count_hits(gw, 1), 0);  // nothing names the hidden shooter
    EXPECT_EQ(count_hits(gw, 2), 0);  // nor tells the shooter whom it hit
}

TEST(WorldFog, ShotsAndAbilityUsesInTheFogAreNotSent) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(hello_event(2, "b"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(spawn_event(2, /*cell=*/99, /*faction=*/2));
    incoming.push(attack_event(2, /*seq=*/1, /*ability=*/2, /*aim_x=*/-1000, /*aim_y=*/0));
    run_ticks(world, 3, 0.05);

    auto seen_by_a = last_snapshot_to(gw, 1);
    ASSERT_TRUE(seen_by_a.has_value());
    EXPECT_EQ(seen_by_a->projectiles_size(), 0);
    EXPECT_TRUE(ability_events(gw, 1).empty());
    auto seen_by_b = last_snapshot_to(gw, 2);
    ASSERT_TRUE(seen_by_b.has_value());
    EXPECT_EQ(seen_by_b->projectiles_size(), 1);  // its own shot, in its sight
    EXPECT_EQ(ability_events(gw, 2).size(), 1u);
}

TEST(WorldFog, AJoinerSeesNothing) {
    lit::TSQueue<lit::ClientEvent> incoming;
    lit::test::MockClientGateway gw;
    auto config = fog_config();
    lit::game::World world(incoming, gw, config);

    incoming.push(hello_event(1, "a"));
    incoming.push(spawn_event(1, /*cell=*/0, /*faction=*/1));
    incoming.push(input_event(1, 0, 0, /*seq=*/1, /*capturing=*/true));
    run_ticks(world, 6, 0.016);  // cell 0 is faction 1's
    incoming.push(hello_event(2, "newcomer"));
    run_ticks(world, 3, 0.016);

    std::optional<::game::v1::MapState> map;
    for (const auto& m : messages_to(gw, 2))
        if (m.has_map_state()) map = m.map_state();
    ASSERT_TRUE(map.has_value());
    for (char owner : map->owners()) EXPECT_EQ(owner, 0);  // the owned cell stays unknown
    auto snap = last_snapshot_to(gw, 2);
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->players_size(), 0);
    EXPECT_EQ(snap->cells_size(), 0);
    EXPECT_EQ(snap->revealed_size(), 0);
}
