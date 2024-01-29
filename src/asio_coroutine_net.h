#ifndef ASIO_COROUTINE_NET_H
#define ASIO_COROUTINE_NET_H

#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>

#include <spdlog/spdlog.h>

namespace hcpp
{
    using namespace asio;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;
    using ssl_socket = ssl::stream<tcp_socket>;

    namespace log=spdlog;
} // namespace hcpp

#endif