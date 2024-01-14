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
    // HACK 好像只能支持发送存在默认构造函数的对象.所以不能使用std::unique_ptr
    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    struct tls_client
    {
        // HACK 如果传递native_handle_,在使用另一个io_context构造新的socket时,需要传递协议,这会使代码成为协议相关的
        // tcp_socket::native_handle_type native_handle_;
        tcp_socket socket_;
        std::string host_;
        std::size_t port_;

        tls_client(tcp_socket socket) : socket_(std::move(socket)) {}
    };

    awaitable<void> https_listen(std::shared_ptr<socket_channel> src);
} // namespace hcpp

#endif