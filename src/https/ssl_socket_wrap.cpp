#include "ssl_socket_wrap.h"
#include "http/thack.h"

#include <asio/streambuf.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/as_tuple.hpp>

#include <spdlog/spdlog.h>

#include <openssl/tls1.h>

namespace hcpp
{
    bool operator<(const ssl_sock_mem::mem_block &fk, int lk) { return !(fk.begin_ + fk.data_.size() > lk); }
    bool operator<(int lk, const ssl_sock_mem::mem_block &fk) { return lk < fk.begin_; }
    bool operator<(const ssl_sock_mem::mem_block &fk1, const ssl_sock_mem::mem_block &fk2) { return fk1.begin_ < fk2.begin_; }

    awaitable<void> ssl_sock_mem::wait()
    {
        co_return;
    }
    awaitable<std::string_view> ssl_sock_mem::async_load_some(std::size_t max_n)
    {
        std::size_t begin = write_index_;
        std::string r{};
        r.resize(max_n);
        r.reserve(max_n);

        auto [e, n] = co_await ssl_sock_->async_read_some(buffer(r, max_n), as_tuple(use_awaitable));

        if (n == 0)
        {
            // XXX 如何关闭呢 https://www.rfc-editor.org/rfc/rfc5246#section-7.2.1
            read_ok_ = false;
            write_ok_ = false;
            ssl_sock_->shutdown();
            co_return "";
        }
        write_index_ += n;
        auto [i, b] = buffs_.emplace(begin, r.substr(0, n));
        assert(b);
        co_return i->data_;
    }

    awaitable<std::size_t> ssl_sock_mem::async_write_some(std::string_view s)
    {
        if (s.empty())
        {
            read_ok_ = false;
            write_ok_ = false;
            ssl_sock_->shutdown();
            co_return 0;
        }

        auto [e, n] = co_await ssl_sock_->async_write_some(buffer(s), as_tuple(use_awaitable));
        co_return n;
    }

    awaitable<std::string_view> ssl_sock_mem::async_load_until(const std::string &d)
    {
        auto bs = 512;
        if (d.size() > bs)
        {
            throw std::runtime_error("too long delimiter");
        }

        std::size_t begin = write_index_;

        std::string r{};
        streambuf buff;

        auto [e, n] = co_await asio::async_read_until(*ssl_sock_, buff, d, as_tuple(use_awaitable));

        if (n == 0)
        {
            read_ok_ = false;
            write_ok_ = false;
            ssl_sock_->shutdown();
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

    awaitable<void> ssl_sock_mem::async_write_all(std::string_view s)
    {
        if (s.empty())
        {
            read_ok_ = false;
            write_ok_ = false;
            ssl_sock_->shutdown();
            co_return;
        }
        auto [e, n] = co_await async_write(*ssl_sock_, buffer(s, s.size()), as_tuple(use_awaitable));
    }

    std::string_view ssl_sock_mem::get_some()
    {
        auto i = buffs_.find(read_index_);
        if (i == buffs_.end())
        {
            return "";
        }
        assert(read_index_ >= i->begin_);
        return std::string_view(i->data_.begin() + (read_index_ - i->begin_), i->data_.end());
    }

    std::size_t ssl_sock_mem::remove_some(std::size_t n)
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

    ssl_sock_mem::ssl_sock_mem(ssl_stream_type stream_type) : stream_type_(stream_type)
    {
    }

    void ssl_sock_mem::check(mem_move &m)
    {
        m.make(std::move(ssl_sock_));
    }

    awaitable<void> ssl_sock_mem::init(tcp_socket &&sock)
    {
        if (stream_type_ == ssl_stream_type::client)
        {
            ctx_ = std::make_unique<ssl::context>(ssl::context::tls);
            // HACK 注意 一定要在使用私钥之前调用
            ctx_->set_password_callback([](auto a, auto b)
                                          { return "123456"; });
            // context.use_certificate_file("output.crt", ssl::context::file_format::pem);
            ctx_->use_certificate_chain_file("server.crt.pem");
            ctx_->use_private_key_file("server.key.pem", asio::ssl::context::pem);
        }
        else
        {
            ctx_ = std::make_unique<ssl::context>(ssl::context::tls_server);
        }
        ssl_sock_ = std::make_unique<ssl_socket>(std::move(sock), *ctx_);
        co_await ssl_sock_->async_handshake(stream_type_);
    }

    awaitable<void> ssl_sock_mem::init(tcp_socket::native_handle_type nh_sock, tcp_socket::protocol_type protocol)
    {
        ctx_ = std::make_unique<ssl::context>(stream_type_ == ssl_stream_type::client ? ssl::context::tls : ssl::context::tls_server);
        auto &&e = co_await this_coro::executor;
        ssl_sock_ = std::make_unique<ssl_socket>(tcp_socket(e, protocol, nh_sock), *ctx_);
        co_await ssl_sock_->async_handshake(stream_type_);
    }

    void ssl_sock_mem::set_sni(std::string sni)
    {
        if (stream_type_ == ssl::stream_base::server)
        {
            spdlog::error("检测ssl::stream_base::server,不设置sni");
        }
        // https://github.com/boostorg/beast/blob/develop/example/http/client/sync-ssl/http_client_sync_ssl.cpp#L73
        if (!SSL_set_tlsext_host_name(ssl_sock_->native_handle(), sni.c_str()))
        {
            spdlog::error("设置sni失败 {}", sni);
            throw std::runtime_error("设置sni失败");
        }
    }

} // namespace hcpp
