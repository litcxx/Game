#include "session.hpp"

#include <boost/beast/core/error.hpp>
#include <spdlog/spdlog.h>

Session::Session(tcp::socket&& socket) :
  websocket_(std::move(socket))
{}

void Session::Run()
{
  spdlog::info("Session::Run");
  // TODO set timeout
  // TODO set decorator

  auto self = shared_from_this();
  websocket_.async_accept(
    [self](const beast::error_code &ec)
    {
      if (ec == websocket::error::closed)
        return spdlog::info("session was closed");
      if (ec)
        return spdlog::error("accept: {}", ec.what());
      self->ReadRequest();
    }
  );
}

void Session::ReadRequest()
{
  spdlog::info("Session::ReadRequest");
  auto self = shared_from_this();
  websocket_.async_read(
    buffer_,
    [self](const beast::error_code& ec, std::size_t size)
    {
      if (ec)
        return spdlog::error("read: {}", ec.what());
      spdlog::info("read: {} bytes from clients", size);
      self->SendData();
    }
  );
}

void Session::SendData()
{
  spdlog::info("Session::SendData");
  // Echo the message
  auto self = shared_from_this();
  websocket_.text(websocket_.got_text());
  websocket_.async_write(
    buffer_.data(),
    [self](const beast::error_code& ec, std::size_t size)
    {
      if (ec)
        return spdlog::error("write: {}", ec.what());
      spdlog::info("send: {} bytes", size);
      self->buffer_.consume(size);
      self->ReadRequest();
    }
  );
}
