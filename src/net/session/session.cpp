#include "session.hpp"

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <spdlog/spdlog.h>

Session::Session(tcp::socket&& socket, ssl::context& ctx) :
  socket_(std::move(socket), ctx)
{}

void Session::Run()
{
  spdlog::info("Session::Run");

  auto self = shared_from_this();
  socket_.next_layer().async_handshake(
    ssl::stream_base::server,
    [self](const beast::error_code& ec)
    {
      if (ec)
        return spdlog::error("handshake: {}", ec.what());
      self->Accept();
    }
  );
}

void Session::Accept()
{

  spdlog::info("Session::Accept");
  // TODO set timeout
  // TODO set decorator

  auto self = shared_from_this();
  socket_.async_accept(
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
  socket_.async_read(
    buffer_,
    [self](const beast::error_code& ec, std::size_t size)
    {
      if (ec == websocket::error::closed)
        return spdlog::info("session was closed");
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
  socket_.text(socket_.got_text());
  socket_.async_write(
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
