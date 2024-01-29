#include "tunnel.h"

#include <string>

#include <spdlog/spdlog.h>

#include <asio/ip/tcp.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>
#include <asio/connect.hpp>
#include <asio/experimental/as_single.hpp>
#include <asio/as_tuple.hpp>

namespace hcpp
{
    using namespace asio;
    namespace log=spdlog;

    using ip::tcp;
    using default_token = use_awaitable_t<>;
    using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
    using tcp_socket = default_token::as_default_on_t<tcp::socket>;

    class simple_tunnel_mem : public simple_mem
    {
    public:
        simple_tunnel_mem(tcp_socket sock, std::size_t n = 1024 * 4 * 4) : sock_(std::move(sock))
        {
            buff_.resize(n);
        }

    public:
        virtual awaitable<void> wait();

        // XXX读取最大max_n的数据,放到buff中.
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n = 1024 * 4);

        virtual awaitable<void> async_write_all(std::string_view);

        std::string buff_;
        tcp_socket sock_;
    };

    awaitable<void> simple_tunnel_mem::wait()
    {
        co_await sock_.async_wait(tcp_socket::wait_write);
        // co_await sock_.async_wait(tcp_socket::wait_read);
        read_ok_ = write_ok_ = true;
        co_return;
    }

    awaitable<std::string_view> simple_tunnel_mem::async_load_some(std::size_t max_n)
    {

        auto mn = buff_.size();
        if (mn > max_n)
        {
            mn = max_n;
        }
        auto [e, n] = co_await sock_.async_read_some(buffer(buff_, mn), as_tuple(use_awaitable));
        if (n == 0)
        {
            read_ok_ = false;
            sock_.shutdown(tcp_socket::shutdown_receive);
        }
        co_return std::string_view{buff_.data(), n};
    }

    awaitable<void> simple_tunnel_mem::async_write_all(std::string_view data)
    {
        if (data.empty())
        {
            sock_.shutdown(tcp_socket::shutdown_send);
            write_ok_ = false;
            co_return;
        }

        auto [e, n] = co_await async_write(sock_, buffer(data), as_tuple(use_awaitable));
        if (n == 0)
        {
            sock_.shutdown(tcp_socket::shutdown_send);
            write_ok_ = false;
        }
    }

    socket_tunnel::socket_tunnel()
    {
    }

    socket_tunnel::~socket_tunnel()
    {
        spdlog::info("{} http_tunnel结束,共发送 {}({}kb) 接收 {}({}kb)", host_, w_n_, w_n_ / 1024, r_n_, r_n_ / 1024);
    }

    awaitable<bool> socket_tunnel::wait(std::shared_ptr<slow_dns> dns)
    {
        auto rrs = dns->resolve_cache({host_, service_});
        if (!rrs)
        {
            rrs.emplace(co_await dns->resolve({host_, service_}));
        }

        auto e = co_await this_coro::executor;
        tcp_socket sock(e);
        if (auto [error, remote_endpoint] = co_await asio::async_connect(sock, *rrs, asio::experimental::as_single(asio::use_awaitable), 0); error)
        {
            spdlog::info("连接远程出错 -> {}:{}", host_, service_);
            dns->remove_svc({host_, service_}, remote_endpoint.address().to_string());
            co_return false;
        }
        s_ = std::make_unique<simple_tunnel_mem>(std::move(sock));
        co_await s_->wait();
        co_return true;
    }

    awaitable<void> socket_tunnel::read()
    {
        auto data = co_await s_->async_load_some(1024 * 4);
        co_await c_->async_write_all(data);
        r_n_ += data.size();
        s_->remove_some(data.size());
    }

    awaitable<void> socket_tunnel::write()
    {
        auto data = co_await c_->async_load_some(1024 * 4);
        co_await s_->async_write_all(data);
        w_n_ += data.size();
        c_->remove_some(data.size());
    }

    namespace
    {
        inline awaitable<void> bind_read(std::shared_ptr<socket_tunnel> self)
        {
            while (self->ok())
            {
                co_await self->read();
            }
            spdlog::debug("读服务端bind关闭");
        }
        inline awaitable<void> bind_write(std::shared_ptr<socket_tunnel> self)
        {
            while (self->ok())
            {
                co_await self->write();
            }
            spdlog::debug("写服务端bind关闭");
        }
    } // namespace

    awaitable<void> socket_tunnel::make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service)
    {
        c_ = m;
        host_ = host;
        service_ = service;
        if (!co_await wait(slow_dns::get_slow_dns()))
        {
            spdlog::error("服务连接失败");
            co_return;
        }

        auto self = shared_from_this();
        auto e = co_await this_coro::executor;
        co_spawn(e, bind_read(self), detached);
        co_spawn(e, bind_write(self), detached);
        
        co_await c_->async_write_all("HTTP/1.1 200 OK\r\n\r\n");
    }

} // namespace hcpp
