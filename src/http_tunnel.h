#ifndef TUNNEL_H
#define TUNNEL_H
#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/as_single.hpp>

#include <memory>

using asio::use_awaitable_t;
using asio::awaitable;

using asio::ip::tcp;

// using default_token = asio::experimental::as_single_t<use_awaitable_t<>>;
using default_token = use_awaitable_t<>;

using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
using tcp_socket = default_token::as_default_on_t<tcp::socket>;


// using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
// using tcp_socket_single = asio::experimental::as_single_t<use_awaitable_t<>>::as_default_on_t<tcp::socket>;



struct tunnel_session
{
    std::shared_ptr<tcp_socket> client;
    std::shared_ptr<tcp_socket> server;
    std::string server_info;
};


awaitable<void> send_session(tunnel_session ts);

awaitable<void> receive_session(tunnel_session ts);

awaitable<void> double_session(tunnel_session ts);

#endif