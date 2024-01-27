#ifndef SRC_PROXY
#define SRC_PROXY

#include "https/server.h"
#include "http/httpclient.h"
#include "http/httpserver.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/concurrent_channel.hpp>

namespace hcpp
{

    using asio::awaitable;
    using namespace asio::experimental;

    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    awaitable<void> http_proxy(http_client client, std::shared_ptr<socket_channel> https_channel);

    class mimt_https_proxy;

    class http_proxy
    {
    public:
        awaitable<void> wait_http(uint16_t port);
        void attach(std::shared_ptr<mimt_https_proxy> mimt);

    private:
        awaitable<void> proxy(http_client client);

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

    class mimt_https_proxy
    {
    public:
        awaitable<void> wait_http(std::shared_ptr<socket_channel> c);
        awaitable<void> wait(uint16_t port);
        awaitable<void> proxy(http_client client);
    };

} // namespace hcpp

#endif /* SRC_PROXY */
