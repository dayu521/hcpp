#ifndef SRC_HTTPSERVER
#define SRC_HTTPSERVER

#include "https/server.h"
#include "http/httpclient.h"
#include "http/http_svc_keeper.h"
#include "http/tunnel.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/concurrent_channel.hpp>

#include <functional> 

namespace hcpp
{

    using asio::awaitable;
    using namespace asio::experimental;

    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    awaitable<void> http_do(http_client client, std::shared_ptr<tunnel> t);

    class httpserver
    {
    public:
        awaitable<void> wait_http(uint16_t port);

        using http_worker=std::function<void(std::shared_ptr<socket_channel>)>;
        void attach(http_worker w);

    private:
        std::shared_ptr<socket_channel> https_channel;
    };

    // struct mimt_client
    // {
    //     // HACK 如果传递native_handle_,在使用另一个io_context构造新的socket时,需要传递协议,这会使代码成为协议相关的
    //     // tcp_socket::native_handle_type native_handle_;
    //     tcp_socket socket_;
    //     std::string host_;
    //     std::size_t port_;

    //     tls_client(tcp_socket socket) : socket_(std::move(socket)) {}
    // };

    class mimt_https_server
    {
    public:
        awaitable<void> wait_http(std::shared_ptr<socket_channel> c);
        awaitable<void> wait(uint16_t port);
    };

} // namespace hcpp

#endif /* SRC_HTTPSERVER */
