#include "mitm_svc.h"
#include "http/tunnel.h"

#include <thread>

#include <spdlog/spdlog.h>
#include <asio/buffer.hpp>
#include <asio/bind_executor.hpp>
#include "mitm_svc.h"

namespace hcpp
{
    namespace log = spdlog;

    using namespace asio::buffer_literals;

    mitm_svc::mitm_svc() : http_svc_keeper(make_threadlocal_svc_cache<mitm_svc>(), slow_dns::get_slow_dns())
    {
    }
    awaitable<std::shared_ptr<memory>> ssl_mem_factory::create(std::string host, std::string service)
    {
        try
        {
            if (auto sock = co_await make_socket(host, service); sock)
            {
                auto ssl_m = std::make_shared<ssl_sock_mem>(asio::ssl::stream_base::client);
                ssl_m->init(std::move(*sock));
                // ssl_m->set_sni("www.baidu.com");
                ssl_m->close_sni();
                co_await ssl_m->async_handshake();
                co_return ssl_m;
            }
            co_return std::shared_ptr<memory>{};
        }
        catch (std::exception &e)
        {
            log::error("ssl_mem_factory::create: {}", e.what());
            throw;
        }
    }

    awaitable<void> channel_tunnel::make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service)
    {
        co_await m->async_write_all("HTTP/1.1 200 OK\r\n\r\n");
        chc_ = std::make_shared<channel_client>();
        chc_->host_ = host;
        chc_->service_ = service;
        m->check(*this);
        co_await channel_->async_send(asio::error_code{}, chc_);
    }

    void channel_tunnel::make(std::unique_ptr<tcp_socket> sock)
    {
        chc_->sock_ = std::move(sock);
        log::info("转移tcp_socket到channel");
    }

    void channel_tunnel::make(std::unique_ptr<ssl_socket> sock)
    {
        log::error("未实现");
        sock->shutdown();
        chc_->sock_ = std::unique_ptr<tcp_socket>();
    }

    http_request https_client::make_request() const
    {
        http_request hr;
        hr.host_ = host_;
        hr.port_ = service_;
        return hr;
    }

    awaitable<std::shared_ptr<memory>> channel_client::make() &&
    {
        auto e = co_await this_coro::executor;
        auto &&protocol = sock_->local_endpoint().protocol();
        auto &&h = sock_->release();
        tcp_socket s(e, protocol, std::move(h));
        // tcp_socket s(e, sock_->local_endpoint().protocol(), sock_->release());
        auto ssl_m = std::make_shared<ssl_sock_mem>(asio::ssl::stream_base::server);
        ssl_m->init(std::move(s));
        co_await ssl_m->async_handshake();
        co_return ssl_m;
    }

} // namespace hcpp
