#include "world.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>

#include "player/player.hpp"
#include "protocol/net_packet.hpp"
#include "protocol/opcodes.hpp"
#include "protocol/server_packet.hpp"
#include "spdlog/common.h"
#include "tile/tile.hpp"
#include "utils/ts_queue.hpp"

namespace lit::game {
World::World(std::shared_ptr<NetworkSubsystem> net_subsystem,
             std::shared_ptr<GameSubsystem> game_subsystem, const GameConfig& config)
    : net_subsystem_(net_subsystem), game_subsystem_(game_subsystem), config_(config) {
    // Initialzie map
    map_.resize(config_.grid_x * config_.grid_y);
    for (std::size_t y = 0; y < config_.grid_y; y++) {
        for (std::size_t x = 0; x < config_.grid_x; x++) {
            std::size_t index = y * config_.grid_x + x;
            // Create tile
            if (config_.map[index] != 0) {
                map_[index] = Tile{static_cast<double>(x) * config_.tile,
                                   static_cast<double>(y) * config_.tile, config_.tile,
                                   config_.tile, TileType::Solid};
            }
        }
    }
}

void World::game_loop() {
    const double tick_seconds = 1.0 / config_.tick_rate;
    auto tick_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(tick_seconds));

    auto last = std::chrono::steady_clock::now();

    for (;;) {
        auto current = std::chrono::steady_clock::now();
        std::chrono::duration<double> delta = current - last;
        last = current;

        // spdlog::info("delta = {}, actual Hz = {}", delta.count(), 1.0 / delta.count());

        tick(delta.count());

        std::this_thread::sleep_until(current + tick_duration);
    }
}

void World::tick(double dt) {
    // Double buffering incoming queue.
    ts_swap(game_subsystem_->in_queue, net_subsystem_->in_queue);

    spdlog::info("queue length: {}", game_subsystem_->in_queue.size());

    spdlog::info("number of players: {}", players_.size());
    for (auto item : players_) {
        item.second->set_vel(0.0, item.second->get_vel_y());
    }

    // Handle player inputs
    while (!game_subsystem_->in_queue.empty()) {
        auto packet = game_subsystem_->in_queue.try_pop();
        if (packet) process_input(std::move(packet.value()), dt);
    }

    // Update physics
    spdlog::info("number of players: {}", players_.size());
    for (auto item : players_) update(*item.second, dt);

    // Push all packets to network queue for broadcast.
    while (!game_subsystem_->out_queue.empty()) {
        auto packet = game_subsystem_->out_queue.try_pop();
        if (packet) net_subsystem_->out_queue.push(std::move(packet.value()));
    }
}

void World::process_input(std::unique_ptr<ServerPacket> packet, double dt) {
    spdlog::info("(World::ProcessInput)");
    // TODO check is this player exists
    auto player = players_[packet->get_id()];
    if (!player)
        spdlog::info("player not found id: {}\nfile: {} line: {}", packet->get_id(), __FILE__,
                     __LINE__);
    auto net_packet = packet->get_net_packet();
    std::uint16_t opcode = net_packet.get_head_opcode();

    double speed = 100.0 * dt;
    double jump_force = 400.0 * dt;

    spdlog::info("Process packet:\nid: {}\nopcode:{}", packet->get_id(),
                 net_packet.get_head_opcode());

    switch (to_opcode(opcode)) {
        case Opcodes::CreatePlayer: {
            spdlog::info("Opcodes::CreatePlayer");
            auto player = std::make_shared<Player>(
                packet->get_id(),
                config_.player.player_start_x * config_.tile + config_.player.player_offset,
                config_.player.player_start_y * config_.tile + config_.player.player_offset, 0.0,
                0.0, config_.player.width, config_.player.height);
            add_player(player);
            break;
        }
        case Opcodes::RemovePlayer:
            spdlog::info("Opcodes::RemovePlayer");
            remove_player(packet->get_id());
            break;
        case Opcodes::MoveLeft:
            spdlog::info("Opcodes::MoveLeft");
            player->set_vel(-speed, player->get_vel_y());
            break;
        case Opcodes::MoveRight:
            spdlog::info("Opcodes::MoveRight");
            player->set_vel(speed, player->get_vel_y());
            break;
        case Opcodes::Jump:
            spdlog::info("Opcodes::Jump");
            if (player->on_ground()) {
                player->set_vel(player->get_vel_x(), -jump_force);
                player->set_on_ground(false);
                spdlog::info("JUMP!");
            }
            break;
        default:
            spdlog::warn("unknown opcode: {}", opcode);
    }
}

void World::update(IPlayer& player, double dt) {
    spdlog::info("(World::Update)");
    const double g = 9.8;
    double vel_y = player.get_vel_y() + g * dt;
    player.set_vel(player.get_vel_x(), vel_y);
    move_player(player);
    // spdlog::info("vel_y: {}", player.GetVelY());

    spdlog::info("make move packet");
    auto move_packet = std::make_unique<ServerPacket>(
        move_player_packet(player.get_id(), player.get_x(), player.get_y()), player.get_id());
    game_subsystem_->out_queue.push(std::move(move_packet));
}

void World::move_player(IPlayer& player) {
    spdlog::info("(World::MovePlayer)");
    double vel_x = player.get_vel_x();
    double vel_y = player.get_vel_y();

    /* ------ X Axis ------*/
    if (vel_x != 0) {
        spdlog::info("============== X AXIS ==============");
        // calculate collision along x axis
        SweptData swept =
            collision_.swept_axis(player, config_.tile, config_.grid_x,
                                  static_cast<std::uint8_t>(config_.grid_y), map_, vel_x, 0.0);

        vel_x *= swept.entry_time;
        player.move(vel_x, 0.0);
        if (swept.hit) {
            player.set_vel(0.0, vel_y);
            // spdlog::info("HIT SIDE WALL\n"
            //              "x: {} y: {}", player.GetX(), player.GetY());
        }
        spdlog::info("============== X AXIS ==============");
    }

    /* ------ Y Axis ------*/
    if (vel_y != 0) {
        spdlog::info("============== Y AXIS ==============");
        // calculate collision along y axis
        SweptData swept =
            collision_.swept_axis(player, config_.tile, config_.grid_x,
                                  static_cast<std::uint8_t>(config_.grid_y), map_, 0.0, vel_y);

        vel_y *= swept.entry_time;
        player.move(0.0, vel_y);
        player.set_on_ground(false);
        if (swept.hit) {
            if (vel_y >= 0.0) {
                player.set_on_ground(true);
                // spdlog::info("HIT GROUND");
            } else {
                // spdlog::info("HIT WALL");
            }
            player.set_vel(vel_x, 0.0);
        }
        spdlog::info("============== Y AXIS ==============");
    }
}

void World::add_player(std::shared_ptr<IPlayer> player) {
    spdlog::info("(World::AddPlayer)");
    {
        std::lock_guard lock(players_mutex_);
        // TODO check if this id already exists
        // Otherwise it overwrite previous player
        players_[player->get_id()] = player;

        spdlog::info("CreatePlayerPacket:\nid: {}\nx: {}\ny: {}\nwidth: {}\nheight: {}",
                     player->get_id(), player->get_x(), player->get_y(), player->get_width(),
                     player->get_height());
        // Create player on client side
        auto create_packet = std::make_unique<ServerPacket>(
            create_player_packet(player->get_id(), player->get_x(), player->get_y(),
                                 player->get_width(), player->get_height()),
            player->get_id(), PacketType::Rpc);

        // spdlog::info("packet body size: {}", packet0.GetBodySize());
        // TODO make it instance send
        game_subsystem_->out_queue.push(std::move(create_packet));

        // Send all players to new player
        NetPacket spawn_packet;
        spawn_packet.set_head_opcode(to_uint16(Opcodes::SpawnPlayers));
        spawn_packet << players_.size() - 1;
        for (const auto& elem : players_) {
            if (elem.first != player->get_id()) {
                spdlog::info("make spawn_packet id: {} x: {} y: {} width: {} height: {}",
                             elem.second->get_id(), elem.second->get_x(), elem.second->get_y(),
                             elem.second->get_width(), elem.second->get_height());

                spawn_packet << elem.second->get_id() << elem.second->get_x()
                             << elem.second->get_y() << elem.second->get_width()
                             << elem.second->get_height();
            }
        }
        game_subsystem_->out_queue.push(std::make_unique<ServerPacket>(
            std::move(spawn_packet), player->get_id(), PacketType::Rpc));
    }

    // // Notify others
    auto add_packet = std::make_unique<ServerPacket>(
        add_player_packet(player->get_id(), player->get_x(), player->get_y(), player->get_width(),
                          player->get_height()),
        player->get_id(), PacketType::RpcOthers);
    game_subsystem_->out_queue.push(std::move(add_packet));
}

void World::remove_player(std::size_t id) {
    spdlog::info("(World::RemovePlayer)");
    std::lock_guard lock(players_mutex_);
    // TODO check if this id is exists
    players_.erase(id);
    auto remove_packet = std::make_unique<ServerPacket>(remove_player_packet(id), id);
    game_subsystem_->out_queue.push(std::move(remove_packet));
}

std::size_t World::player_numbers() const {
    std::lock_guard lock(players_mutex_);
    return players_.size();
}
}  // namespace lit::game
