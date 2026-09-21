#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace lit::net {
class ISocket {
  public:
    using ReadHandler = std::function<void(const std::error_code& ec, std::size_t size)>;
    using CompletionHandler = std::function<void(const std::error_code& ec)>;
    virtual ~ISocket() = default;

    virtual void async_accept(CompletionHandler handler) = 0;
    virtual void async_read_some(std::uint8_t* buffer, std::size_t limit, ReadHandler handler) = 0;
    virtual void async_write(const std::uint8_t* buffer, std::size_t limit,
                             ReadHandler handler) = 0;

    virtual std::string string_address() = 0;
    virtual void close() = 0;
};
}  // namespace lit::net
