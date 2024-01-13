#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#include "asio_coroutine_net.h"
#include "dns.h"

#include <string>

#include <asio/ssl.hpp>
#include <asio/experimental/concurrent_channel.hpp>

namespace hcpp
{

    using namespace asio;
    using namespace experimental;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

    struct tls_client;
    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, tls_client)>>;

    struct tls_client
    {
        tcp_socket::native_handle_type native_handle_;
        std::string host_;
        std::size_t port_;
    };

    awaitable<void> https_listen(std::shared_ptr<socket_channel> src);
} // namespace hcpp

#endif