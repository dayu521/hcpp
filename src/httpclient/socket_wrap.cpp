#include "socket_wrap.h"

#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/experimental/awaitable_operators.hpp>

namespace hcpp
{
    awaitable<std::size_t> hcpp::socket_memory::async_read_some(std::string & buff,std::size_t max_n,std::size_t begin=0)
    {
        auto n = buff_.size();
        std::string r{};
        if (n == 0)
        {
            if (eof())
            {
                co_return r;
            }
            r.reserve(max_n);
            auto rn=co_await sock_->async_read_some(buffer(r,max_n));
            r.resize(rn);
            n = buff_.size();
        }
        if (n > max_n)
        {
            n = max_n;
        }
        r.reserve(n);
        buffer_copy(buffer(r), buff_.data(), n);
        buff_.consume(n);
        co_return std::move(r);
    }

    awaitable<std::size_t> hcpp::socket_memory::async_write_some(const std::string &s)
    {
        //HACK 不需要放到缓存
        auto n = co_await sock_->async_write_some(buffer(s));
        co_return n;
    }

    awaitable<std::pair<std::string,std::size_t>> hcpp::socket_memory::async_read_until(const std::string &d)
    {
        if (d.size() > 256)
        {
            throw std::runtime_error("too long delimiter");
        }

        std::string r{};

        if (buff_.size() == 0)
        {
            auto n = co_await asio::async_read_until(*sock_, buff_, d);
            r.reserve(n);
            buffer_copy(buffer(r), buff_.data(), n);
            buff_.consume(n);
            co_return std::move(r);
        }

        std::string some_str{};
        auto j=0;
        while (true)
        {
            auto sub_buff=buff_.data();
            for (j; j < buff_.size();)
            {
                some_str.append(buffers_begin(sub_buff)+j, buffers_end(sub_buff) + 256);
                if (auto np = some_str.find(d, j - d.size()); np != std::string::npos)
                {
                    buff_.consume(j+np);
                    co_return some_str.substr(0, j+np);
                }
                j+=256;
            }
            co_await sock_->async_write_some(buff_);
        }
        // TODO kmp后缀匹配,如果使用async_read_until
    }

    awaitable<void> hcpp::socket_memory::async_write_all(const std::string &s)
    {
        co_await async_write(*sock_, buffer(s, s.size()));
    }

    bool hcpp::socket_memory::eof()
    {
        return buff_.size() == 0 && !sock_->is_open();
    }
} // namespace hcpp