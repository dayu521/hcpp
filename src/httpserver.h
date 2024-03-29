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
#include <memory>
#include <set>

namespace hcpp
{
    using asio::awaitable;
    using namespace asio::experimental;

    // using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    class http_handler
    {
    public:
        void add_handler(std::string_view path, std::function<std::string (std::string_view)> handler);
        awaitable<void> http_do(std::unique_ptr<http_client> client, std::shared_ptr<service_keeper> sk);
    private:
        std::unordered_map<std::string_view, std::function<std::string (std::string_view)>> base_handlers_;
    };

    awaitable<void> http_do(std::unique_ptr<http_client> client, std::shared_ptr<service_keeper> sk);

    using tunnel_advice = std::function<std::shared_ptr<tunnel>(std::shared_ptr<tunnel>, std::string_view, std::string_view)>;

    class hack_sk : public service_keeper
    {
    public:
        virtual awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service) override
        {
            co_return co_await sk_->wait(svc_host, svc_service);
        }

        virtual awaitable<std::shared_ptr<tunnel>> wait_tunnel(std::string svc_host, std::string svc_service) override
        {
            auto r = co_await sk_->wait_tunnel(svc_host, svc_service);
            r = ta_(r, svc_host, svc_service);
            co_return r;
        }

    public:
        hack_sk(std::unique_ptr<service_keeper> sk, tunnel_advice ta) : sk_(std::move(sk)), ta_(ta) {}
        std::unique_ptr<service_keeper> sk_;
        tunnel_advice ta_;
    };

    class httpserver
    {
    public:
        awaitable<void> wait_http(uint16_t port,io_context & ic);

        // TODO 让mitm替换掉tunnel实现,从而拦截默认tunnel
        void attach_tunnel(tunnel_advice w);

    private:
        tunnel_advice ta_ = [](auto &&r, auto, auto)
        { return r; };
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

    struct subject_identify;
    struct proxy_service;

    class mimt_https_server
    {
    public:
        awaitable<void> wait_c(std::vector<proxy_service> ps);
        awaitable<void> wait(uint16_t port);

        std::optional<std::shared_ptr<tunnel>> find_tunnel(std::string_view svc_host, std::string_view svc_service);

        std::set<std::pair<std::string, std::string>> tunnel_set_;

        void set_ch(std::shared_ptr<socket_channel> ch);

        void set_ca(subject_identify ca_subject);
        void set_root_verify_store_path(std::string_view root_verify_store_path);

    private:
        std::shared_ptr<socket_channel> channel_;
        std::shared_ptr<subject_identify> ca_subject_;
        std::string root_verify_store_path_;
    };

    using notify_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::string)>>;

} // namespace hcpp

#endif /* SRC_HTTPSERVER */
