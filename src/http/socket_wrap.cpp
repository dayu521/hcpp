#include "socket_wrap.h"

#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/experimental/awaitable_operators.hpp>

namespace hcpp
{

    // using mem_block = socket_memory::mem_block;

    bool operator<(const socket_memory::mem_block &fk, int lk) { return !(fk.begin_ + fk.data_.size() > lk); }
    bool operator<(int lk, const socket_memory::mem_block &fk) { return lk < fk.begin_; }
    bool operator<(const socket_memory::mem_block &fk1, const socket_memory::mem_block &fk2) { return fk1.begin_ < fk2.begin_; }

    awaitable<void> socket_memory::wait()
    {
        co_await sock_->async_wait(tcp_socket::wait_read);
    }

    awaitable<std::string_view> hcpp::socket_memory::async_load_some(std::size_t max_n)
    {
        std::size_t begin = write_index_;
        std::string r{};
        r.resize(max_n);
        r.reserve(max_n);
        std::size_t n = 0;

        try
        {
            n = co_await sock_->async_read_some(buffer(r, max_n));
        }
        catch (const std::exception &e)
        {
            read_ok_ = false;
            throw e;
        }

        if (n == 0)
        {
            read_ok_ = false;
            co_return "";
        }
        write_index_ += n;
        auto [i, b] = buffs_.emplace(begin, r.substr(0, n));
        assert(b);
        co_return i->data_;
    }

    awaitable<std::size_t> hcpp::socket_memory::async_write_some(std::string_view s)
    {
        std::size_t n = 0;
        try
        {
            // HACK 不需要放到缓存
            n = co_await sock_->async_write_some(buffer(s));
        }
        catch (const std::exception &e)
        {
            write_ok_ = false;
            throw e;
        }

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

        std::size_t n = 0;
        try
        {
            n = co_await asio::async_read_until(*sock_, buff, d);
        }
        catch (const std::exception &e)
        {
            read_ok_ = false;
            throw e;
        }
        if (n == 0)
        {
            read_ok_ = false;
            co_return "";
        }
        r.resize(buff.size());
        r.reserve(buff.size());
        buffer_copy(buffer(r), buff.data(), buff.size());
        buff.consume(buff.size());
        write_index_ += r.size();
        auto [i, b] = buffs_.insert({begin, std::move(r)});
        assert(b);
        co_return std::string_view{i->data_.data(), n};
    }

    awaitable<void> hcpp::socket_memory::async_write_all(std::string_view s)
    {
        try
        {
            co_await async_write(*sock_, buffer(s, s.size()));
        }
        catch (const std::exception &e)
        {
            write_ok_ = false;
            throw e;
        }
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

    void socket_memory::remove_some(std::size_t n)
    {
        read_index_ += n;
        for (auto i = buffs_.begin(); i != buffs_.end();)
        {
            if (read_index_ < i->begin_ + i->data_.size())
            {
                break;
            }
            auto old = i;
            i++;
            buffs_.erase(old);
        }
    }

    bool hcpp::socket_memory::ok()
    {
        return read_ok_ && write_ok_;
    }
} // namespace hcpp