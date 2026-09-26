#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
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
