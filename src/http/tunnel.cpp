#include "tunnel.h"

#include <string>

#include <spdlog/spdlog.h>

#include <asio/ip/tcp.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>
#include <asio/connect.hpp>
#include <asio/experimental/as_single.hpp>

namespace hcpp
{
    using namespace asio;

    using ip::tcp;
    using default_token = use_awaitable_t<>;
    using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
    using tcp_socket = default_token::as_default_on_t<tcp::socket>;

    class simple_tunnel_mem : public memory
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
        try
        {
            auto mn = buff_.size();
            if (mn > max_n)
            {
                mn = max_n;
            }
            auto n = co_await sock_.async_read_some(buffer(buff_, mn));
            if (n == 0)
            {
                read_ok_ = false;
                sock_.shutdown(tcp_socket::shutdown_receive);
            }
            co_return std::string_view{buff_.data(), n};
        }
        catch (const std::exception &e)
        {
            read_ok_ = false;
            throw e;
        }
    }

    awaitable<void> simple_tunnel_mem::async_write_all(std::string_view data)
    {
        try
        {
            co_await async_write(sock_, buffer(data));
        }
        catch (const std::exception &e)
        {
            sock_.shutdown(tcp_socket::shutdown_send);
            write_ok_ = false;
            throw e;
        }
    }

    http_tunnel::http_tunnel(std::shared_ptr<memory> m, std::string host, std::string service) : c_(m), host_(host), service_(service)
    {
    }

    http_tunnel::~http_tunnel()
    {
        spdlog::info("{} http_tunnel结束,共发送 {}({}kb) 接收 {}({}kb)", host_, w_n_, w_n_ / 1024, r_n_, r_n_ / 1024);
    }

    awaitable<bool> http_tunnel::wait(std::shared_ptr<slow_dns> dns)
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

    awaitable<void> http_tunnel::bind_write(std::shared_ptr<http_tunnel> self)
    {
        while (self->ok())
        {
            try
            {
                co_await self->write();
            }
            catch (const std::exception &e)
            {
                break;
            }
        }
        co_await self->close_s();
    }

    awaitable<void> http_tunnel::bind_read(std::shared_ptr<http_tunnel> self)
    {
        while (self->ok())
        {
            try
            {
                co_await self->read();
            }
            catch (const std::exception &e)
            {
                break;
            }
        }
        co_await self->close_c();
    }

    awaitable<void> http_tunnel::read()
    {
        try
        {
            auto data = co_await s_->async_load_some(1024 * 4);
            co_await c_->async_write_all(data);
            r_n_ += data.size();
            s_->remove_some(data.size());
            if (data.size() == 0)
            {
                spdlog::debug("服务端关闭连接");
                co_await c_->async_write_some("");
                read_ok_ = false;
            }
        }
        catch (const std::exception &e)
        {
            read_ok_ = false;
            // throw e;
        }
    }

    awaitable<void> http_tunnel::write()
    {
        try
        {
            auto data = co_await c_->async_load_some(1024 * 4);
            co_await s_->async_write_all(data);
            w_n_ += data.size();
            c_->remove_some(data.size());
            if (data.size() == 0)
            {
                spdlog::debug("客户端关闭连接");
                co_await s_->async_write_some("");
                write_ok_ = false;
            }
        }
        catch (const std::exception &e)
        {
            write_ok_ = false;
            // throw e;
        }
    }

    awaitable<void> http_tunnel::make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service, std::shared_ptr<slow_dns> dns)
    {
        auto ts = std::make_shared<http_tunnel>(m, host, service);
        if (!co_await ts->wait(dns))
        {
            spdlog::error("服务连接失败");
            co_return;
        }
        auto e = co_await this_coro::executor;
        co_spawn(e, http_tunnel::bind_read(ts), detached);
        co_spawn(e, http_tunnel::bind_write(ts), detached);
    }

    awaitable<void> http_tunnel::close_c()
    {
        co_await c_->async_write_all("");
    }

    awaitable<void> http_tunnel::close_s()
    {
        co_await s_->async_write_all("");
    }

} // namespace hcpp
