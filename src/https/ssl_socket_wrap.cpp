#include "ssl_socket_wrap.h"
#include "http/thack.h"
#include "os/common.h"

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
            co_return "";
        }
        write_index_ += n;
        auto [i, b] = buffs_.emplace(begin, r.substr(0, n));
        assert(b);
        co_return i->data_;
    }

    awaitable<std::size_t> ssl_sock_mem::async_write_some(std::string_view s)
    {
        auto [e, n] = co_await ssl_sock_->async_write_some(buffer(s), as_tuple(use_awaitable));
        if (s.empty())
        {
            write_ok_ = false;
            ssl_sock_->shutdown();
            co_return 0;
        }
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
            co_return "";
        }
        r.resize(buff.size());
        r.reserve(buff.size());
        buffer_copy(buffer(r), buff.data(), buff.size());
        buff.consume(buff.size());
        write_index_ += r.size();
        auto [i, b] = buffs_.insert({begin, std::move(r)});
        co_return std::string_view{i->data_.data(), n};
    }

    awaitable<void> ssl_sock_mem::async_write_all(std::string_view s)
    {
        try
        {
            co_await async_write(*ssl_sock_, buffer(s, s.size()));
        }
        catch (const std::exception &e)
        {
            log::error("ssl_sock_mem::async_write_all error: {} \n{}", e.what(), s);
            write_ok_ = false;
        }

        // auto [e, n] = co_await async_write(*ssl_sock_, buffer(s, s.size()), as_tuple(use_awaitable));
        if (s.empty())
        {
            write_ok_ = false;
            ssl_sock_->shutdown();
        }
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

    std::size_t ssl_sock_mem::merge_some()
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

    void ssl_sock_mem::reset()
    {
        read_index_ = write_index_ = 0;
        buffs_.clear();
    }

    ssl_sock_mem::ssl_sock_mem(std::string_view host, std::string_view svc) : host_(host), svc_(svc)
    {
    }

    void ssl_sock_mem::check(mem_move &m)
    {
        m.make(std::move(ssl_sock_));
    }

    void ssl_sock_mem::init_server(tcp_socket &&sock, subject_identify si)
    {
        stream_type_ = ssl_stream_type::server;
        ctx_ = std::make_unique<ssl::context>(ssl::context::tls_server);
        // HACK 注意 一定要在使用私钥之前调用
        // ctx_->set_password_callback([](auto a, auto b)
        //                             { return "123456"; });
        // context.use_certificate_file("output.crt", ssl::context::file_format::pem);
        ctx_->use_certificate_chain(asio::buffer(si.cert_pem_));
        ctx_->use_private_key(asio::buffer(si.pkey_pem_), asio::ssl::context::pem);
        ssl_sock_ = std::make_unique<ssl_socket>(std::move(sock), *ctx_);
    }

    void ssl_sock_mem::init_client(tcp_socket &&sock)
    {
        stream_type_ = ssl_stream_type::client;
        ctx_ = std::make_unique<ssl::context>(ssl::context::tls);
        ssl_sock_ = std::make_unique<ssl_socket>(std::move(sock), *ctx_);
    }

    awaitable<void> ssl_sock_mem::async_handshake()
    {
        co_await ssl_sock_->async_handshake(stream_type_);
    }

    void ssl_sock_mem::set_sni(std::string sni)
    {
        if (stream_type_ == ssl::stream_base::server)
        {
            spdlog::error("检测ssl::stream_base::server,不设置sni");
            return;
        }
        // https://github.com/boostorg/beast/blob/develop/example/http/client/sync-ssl/http_client_sync_ssl.cpp#L73
        if (!SSL_set_tlsext_host_name(ssl_sock_->native_handle(), sni.c_str()))
        {
            spdlog::error("设置sni失败 {}", sni);
            throw std::runtime_error("设置sni失败");
        }
    }

    void ssl_sock_mem::close_sni()
    {
        if (!SSL_CTX_set_tlsext_servername_callback(ctx_->native_handle(), nullptr))
        {
            spdlog::error("关闭sni失败");
            throw std::runtime_error("关闭sni失败");
        }
    }

    void ssl_sock_mem::set_verify_callback(verify_callback cb, std::optional<std::string> verify_path)
    {
        if (!verify_path)
        {
            set_platform_default_verify_store(*ctx_);
        }
        else
        {
            ctx_->add_verify_path(*verify_path);
        }
        ssl_sock_->set_verify_mode(ssl::verify_peer);
        ssl_sock_->set_verify_callback(cb);
    }

    ssl_sock_mem::~ssl_sock_mem()
    {
        try
        {
            ssl_sock_->lowest_layer().close();
        }
        catch (const std::exception &e)
        {
            log::error("~ssl_sock_mem: {}", e.what());
        }
        log::info("~ssl_sock_mem: {}=>{}", host_, stream_type_ == ssl::stream_base::server ? "server" : "client");
    }

} // namespace hcpp
