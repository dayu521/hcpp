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
        if (eof())
        {
            co_return "";
        }
        std::size_t begin = 0;
        if (auto it_max = buffs_.cbegin(); it_max != buffs_.cend())
        {
            begin = it_max->begin_ + it_max->data_.size();
        }
        std::string r{};
        r.reserve(max_n);
        auto n = co_await sock_->async_read_some(buffer(r, max_n));
        auto [i,b]=buffs_.emplace(begin, r.substr(0, n));
        assert(b);
        co_return i->data_;
    }

    awaitable<std::size_t> hcpp::socket_memory::async_write_some(const std::string &s)
    {
        // HACK 不需要放到缓存
        auto n = co_await sock_->async_write_some(buffer(s));
        co_return n;
    }

    awaitable<std::string_view> hcpp::socket_memory::async_load_until(const std::string &d)
    {
        auto bs = 512;
        if (d.size() > bs)
        {
            throw std::runtime_error("too long delimiter");
        }
        if (eof())
        {
            co_return "";
        }
        std::size_t begin = 0;
        if (auto it_max = buffs_.cbegin(); it_max != buffs_.cend())
        {
            begin = it_max->begin_ + it_max->data_.size();
        }

        std::string r{};
        streambuf buff;

        auto n = co_await asio::async_read_until(*sock_, buff, d);
        r.resize(buff.size());
        r.reserve(buff.size());
        buffer_copy(buffer(r), buff.data(), buff.size());
        buff.consume(buff.size());
        auto [i,b]=buffs_.insert({begin, std::move(r)});
        assert(b);
        co_return i->data_;
    }

    awaitable<void> hcpp::socket_memory::async_write_all(const std::string &s)
    {
        co_await async_write(*sock_, buffer(s, s.size()));
    }

    std::string_view socket_memory::get_some()
    {
        auto i=buffs_.find(index_);
        if(i==buffs_.end()){
            return "";
        }
        return std::string_view(i->data_.begin()+(index_-i->begin_),i->data_.end());
    }

    void socket_memory::remove_some(std::size_t index)
    {
        for(auto && i : buffs_){
            if(i.begin_+i.data_.size()<index){
                buffs_.erase(i);
            }else{
                break;
            }
        }
        index_+=index;
    }

    bool hcpp::socket_memory::eof()
    {
        return buffs_.size() == 0 && !sock_->is_open();
    }
} // namespace hcpp