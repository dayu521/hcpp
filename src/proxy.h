#ifndef PROXY_H
#define PROXY_H

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

using asio::awaitable;
using asio::use_awaitable_t;
using asio::ip::tcp;

using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;


awaitable<void> read_http_input(tcp_socket socket);

#endif