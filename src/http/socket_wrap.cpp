#include "socket_wrap.h"
#include "thack.h"
#include "dns.h"

#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/as_single.hpp>
#include <asio/streambuf.hpp>
#include <asio/as_tuple.hpp>
#include <asio/connect.hpp>

#include <spdlog/spdlog.h>

namespace hcpp
{

    // using mem_block = socket_memory::mem_block;

    bool operator<(const socket_memory::mem_block &fk, int lk) { return !(fk.begin_ + fk.data_.size() > lk); }
    bool operator<(int lk, const socket_memory::mem_block &fk) { return lk < fk.begin_; }
    bool operator<(const socket_memory::mem_block &fk1, const socket_memory::mem_block &fk2) { return fk1.begin_ < fk2.begin_; }

    awaitable<bool> socket_memory::wait()
    {
        co_await sock_->async_wait(tcp_socket::wait_read);
        co_return ok();
    }

    awaitable<std::string_view> hcpp::socket_memory::async_load_some(std::size_t max_n)
    {
        std::size_t begin = write_index_;
        std::string r{};
        r.resize(max_n);
        r.reserve(max_n);

        auto [e, n] = co_await sock_->async_read_some(buffer(r, max_n), as_tuple(use_awaitable));

        if (n == 0)
        {
            read_ok_ = false;
            // sock_->shutdown(tcp_socket::shutdown_receive);
            co_return "";
        }
        write_index_ += n;
        auto [i, b] = buffs_.emplace(begin, r.substr(0, n));
        assert(b);
        co_return i->data_;
    }

    awaitable<std::size_t> hcpp::socket_memory::async_write_some(std::string_view s)
    {
        if (s.empty())
        {
            write_ok_ = false;
            sock_->shutdown(tcp_socket::shutdown_send);
            co_return 0;
        }

        // HACK 不需要放到缓存
        auto [e, n] = co_await sock_->async_write_some(buffer(s), as_tuple(use_awaitable));

        co_return n;
    }

    awaitable<std::string_view> hcpp::socket_memory::async_load_until(const std::string &d)
    {
        auto bs = 512;
        if (d.size() > bs)
        {
            throw std::runtime_error("too long delimiter");
        }

        std::size_t begin = write_index_;

        std::string r{};
        streambuf buff;

        auto [e, n] = co_await asio::async_read_until(*sock_, buff, d, as_tuple(use_awaitable));

        if (n == 0)
        {
            read_ok_ = false;
            // sock_->shutdown(tcp_socket::shutdown_receive);
            co_return "";
        }
        r.resize(buff.size());
        r.reserve(buff.size());
        buffer_copy(buffer(r), buff.data(), buff.size());
        buff.consume(buff.size());
        write_index_ += r.size();
        auto [i, b] = buffs_.insert({begin, std::move(r)});
        // assert(b);
        // BUG 为什么有时候断言会失败
        co_return std::string_view{i->data_.data(), n};
    }

    awaitable<void> hcpp::socket_memory::async_write_all(std::string_view s)
    {
        if (s.empty())
        {
            write_ok_ = false;
            sock_->shutdown(tcp_socket::shutdown_send);
            co_return;
        }

        auto [e, n] = co_await async_write(*sock_, buffer(s, s.size()), as_tuple(use_awaitable));
    }

    std::string_view socket_memory::get_some()
    {
        auto i = buffs_.find(read_index_);
        if (i == buffs_.end())
        {
            return "";
        }
        assert(read_index_ >= i->begin_);
        return std::string_view(i->data_.begin() + (read_index_ - i->begin_), i->data_.end());
    }

    std::size_t socket_memory::remove_some(std::size_t n)
    {
        read_index_ += n;
        decltype(read_index_) mn = 0;
        for (auto i = buffs_.begin(); i != buffs_.end();)
        {
            if (read_index_ < i->begin_ + i->data_.size())
            {
                break;
            }
            mn += i->data_.size();
            auto old = i;
            i++;
            buffs_.erase(old);
        }
        return mn;
    }

    std::size_t socket_memory::merge_some()
    {
        if (buffs_.size() <= 1)
        {
            return 0;
        }
        std::string::size_type sn = 0;

        for (auto &&i : buffs_)
        {
            sn += i.data_.size();
        }
        std::string data;
        data.reserve(sn);
        for (auto &&i : buffs_)
        {
            data.append(i.data_);
        }
        mem_block mb{};
        mb.begin_ = buffs_.begin()->begin_;
        mb.data_ = std::move(data);
        buffs_.clear();
        buffs_.emplace(std::move(mb));
        return sn;
    }

    void socket_memory::check(mem_move &m)
    {
        m.make(std::move(sock_));
    }

    awaitable<std::optional<tcp_socket>> make_socket(std::string host, std::string service)
    {
        auto svc = host + ":" + service;
        auto dns = slow_dns::get_slow_dns();
        auto rrs = dns->resolve_cache({host, service});
        if (!rrs)
        {
            rrs.emplace(co_await dns->resolve({host, service}));
        }

        auto e = co_await this_coro::executor;
        tcp_socket sock(e);
        sock.open(ip::tcp::v4());
        sock.set_option(socket_base::keep_alive(true));

        if (auto [error, remote_endpoint] = co_await asio::async_connect(sock, *rrs, asio::experimental::as_single(asio::use_awaitable), 0); error)
        {
            spdlog::info("连接远程出错 -> {}", svc);
            hcpp::slow_dns::get_slow_dns()->remove_svc({host, service}, remote_endpoint.address().to_string());
            co_return std::nullopt;
        }
        co_return std::make_optional(std::move(sock));
    }

} // namespace hcpp