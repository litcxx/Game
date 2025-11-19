#pragma once

#include <cstdint>
#include <memory>

namespace ep
{
  class Config {
  public:
    // Filename is only used on the first call.
    static std::shared_ptr<Config> GetInstance(std::string filename = "null");

    const std::string& GetServerIP() const noexcept { return ip_; }
    uint16_t GetServerPort() const noexcept { return port_; }
    uint16_t GetIOThreads() const noexcept { return io_threads_; }
    uint8_t GetTickRate() const noexcept { return tick_rate_; }
  private:
    explicit Config(std::string filename);  

    // Network.
    std::string ip_;
    uint16_t port_;
    uint16_t io_threads_;

    // Game.
    uint8_t tick_rate_;
  };
}
