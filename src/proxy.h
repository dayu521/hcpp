#ifndef PROXY_H
#define PROXY_H

#include "https/server.h"
#include "http/httpclient.h"
#include "http/httpserver.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

namespace hcpp
{

    using asio::awaitable;
    using asio::use_awaitable_t;
    using asio::ip::tcp;

    using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;

    class slow_dns;
    awaitable<void> read_http_input(tcp_socket socket, std::shared_ptr<slow_dns> sdns,std::shared_ptr<socket_channel> https_channel);

    awaitable<void> http_service(http_client client, std::shared_ptr<http_server> server,std::shared_ptr<socket_channel> https_channel);

} // namespace hcpp

#endif