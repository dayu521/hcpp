#ifndef SRC_HTTPSERVER
#define SRC_HTTPSERVER

#include "https/mitm_svc.h"
#include "http/httpclient.h"
#include "http/tunnel.h"
#include "http/service_keeper.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/concurrent_channel.hpp>

#include <functional>

namespace hcpp
{

    using asio::awaitable;
    using namespace asio::experimental;

    // using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    awaitable<void> http_do(http_client client, std::unique_ptr<service_keeper> sk);

    class httpserver
    {
    public:
        awaitable<void> wait_http(uint16_t port);

        using tunnel_advice = std::function<std::shared_ptr<tunnel>(std::shared_ptr<tunnel>)>;

        // TODO 让mitm替换掉tunnel实现,从而拦截默认tunnel
        void attach_tunnel(tunnel_advice w);

    private:
        std::unique_ptr<sk_factory> tunnel_advice_;
    };

    class sk_factory_imp : public sk_factory
    {
    public:
        virtual std::shared_ptr<service_keeper> create() override;

    private:
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
