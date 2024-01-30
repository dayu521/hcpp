#include "http_svc_keeper.h"
#include "socket_wrap.h"

#include <mutex>
#include <queue>

// FIXME 注意,如果包含coro,编译失败,asio 1.28
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/experimental/as_single.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/connect.hpp>

#include <spdlog/spdlog.h>

namespace hcpp
{
    http_svc_keeper::http_svc_keeper(std::shared_ptr<svc_cache> cache, std::shared_ptr<slow_dns> dns) : slow_dns_(dns), endpoint_cache_(cache)
    {
    }

    awaitable<std::shared_ptr<memory>> http_svc_keeper::wait(std::string svc_host, std::string svc_service)
    {
        auto svc_endpoint = co_await endpoint_cache_->get_endpoint(svc_host, svc_service, slow_dns_);

        co_return std::make_shared<service_worker>(svc_endpoint, svc_host, svc_service, endpoint_cache_);
    }

    awaitable<std::shared_ptr<tunnel>> http_svc_keeper::wait_tunnel(std::string svc_host, std::string svc_service)
    {
        co_return std::make_shared<socket_tunnel>();
    }

    using asio::experimental::concurrent_channel;
    using channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<memory>)>>;
    using asio::co_spawn;
    using asio::detached;
    using asio::use_awaitable;

    struct svc_cache::imp
    {
        std::shared_mutex shm_c_;
        std::unordered_map<std::string, std::shared_ptr<memory>> cache_;

        std::shared_mutex shm_rq_;
        std::unordered_map<std::string, std::queue<std::shared_ptr<channel>>> request_queue_;

        std::unique_ptr<mem_factory> mem_factory_;

        awaitable<void> get_endpoint(const std::string &host, const std::string &service, std::shared_ptr<hcpp::slow_dns> dns, std::shared_ptr<channel> c);
        void return_back(const std::string &host, const std::string &service, std::shared_ptr<memory> m);
        bool remove_endpoint(std::string host, std::string service);
    };

    class failed_mem : public simple_mem
    {
        virtual bool ok()
        {
            return false;
        }
    };

    awaitable<void>
    svc_cache::imp::get_endpoint(const std::string &host, const std::string &service, std::shared_ptr<hcpp::slow_dns> dns, std::shared_ptr<channel> c)
    {
        auto svc = host + ":" + service;
        decltype(cache_)::iterator ib;
        {
            std::shared_lock<std::shared_mutex> lock(shm_c_);
            ib = cache_.find(svc);
        }
        std::shared_ptr<memory> m;
        if (ib == cache_.end() || !ib->second->ok())
        {
            if (ib != cache_.end())
            {
                spdlog::info("endpoint_cache : {} 端点无法继续服务,移除", svc);
                std::unique_lock<std::shared_mutex> lock(shm_c_);
                cache_.erase(ib);
            }

            auto v = co_await mem_factory_->create(host, service);
            if (!v)
            {
                c->close();
                return_back(host, service, std::make_shared<failed_mem>());
                co_return;
            }
            {
                std::unique_lock<std::shared_mutex> lock(shm_c_);
                // 插入成功失败都无所谓
                cache_.insert({svc, v});
            }
            m = v;
        }
        else
        {
            m = ib->second;
        }

        auto just_wait = false;
        std::queue<std::shared_ptr<channel>> q;
        q.push(c);
        {
            std::unique_lock<std::shared_mutex> lock(shm_rq_);
            auto svc_q = request_queue_.find(svc);
            if (svc_q != request_queue_.end())
            {
                svc_q->second.push(c);
                just_wait = true;
            }
            else
            {
                request_queue_.emplace(svc, std::move(q));
            }
        }

        if (!just_wait)
        {
            co_await c->async_send(asio::error_code{}, m);
        }
    }

    void svc_cache::imp::return_back(const std::string &host, const std::string &service, std::shared_ptr<memory> m)
    {
        m->reset();
        auto svc = host + ":" + service;
        if (m->ok())
        {
            std::unique_lock<std::shared_mutex> lock(shm_rq_);
            auto ib = request_queue_.find(svc);
            assert(ib != request_queue_.end());
            auto &&q = ib->second;
            assert(!q.empty());
            q.pop();
            while (!q.empty())
            {
                auto c = q.front();

                if (c->try_send(asio::error_code{}, m))
                {
                    return;
                }
                else
                {
                    q.pop();
                    spdlog::error("发送下一个等待者失败,移除此等待者: {}", svc);
                }
            }
            request_queue_.erase(ib);
        }
        else
        {
            decltype(request_queue_)::value_type::second_type q{};
            {
                std::unique_lock<std::shared_mutex> lock(shm_rq_);
                if (auto ib = request_queue_.find(svc); ib != request_queue_.end())
                {
                    q = std::move(ib->second);
                    request_queue_.erase(ib);
                }
            }
            while (!q.empty())
            {
                q.front()->close();
                q.pop();
            }
            spdlog::info("endpoint_cache : {} 端点异常,移除等待中的client", svc);
        }
    }

    bool svc_cache::imp::remove_endpoint(std::string host, std::string service)
    {
        auto svc = host + ":" + service;
        {
            std::unique_lock<std::shared_mutex> lock(shm_c_);
            if (auto ib = cache_.find(svc); ib != cache_.end())
            {
                cache_.erase(ib);
                return true;
            }
        }
        return false;
    }

    awaitable<std::shared_ptr<memory>> svc_cache::get_endpoint(std::string host, std::string service, std::shared_ptr<slow_dns> dns)
    {
        auto ex = co_await asio::this_coro::executor;
        auto ch = std::make_shared<channel>(ex, 1);
        co_spawn(ex, imp_->get_endpoint(host, service, dns, ch), detached);
        co_return co_await ch->async_receive();
    }

    void svc_cache::return_back(std::string host, std::string service, std::shared_ptr<memory> m)
    {
        imp_->return_back(host, service, m);
    }

    bool svc_cache::remove_endpoint(std::string host, std::string service)
    {
        return imp_->remove_endpoint(host, service);
    }

    svc_cache::svc_cache() : imp_(std::make_shared<imp>())
    {
    }

    svc_cache::svc_cache(std::unique_ptr<mem_factory> mem_factory) : imp_(std::make_shared<imp>())
    {
        imp_->mem_factory_ = std::move(mem_factory);
        spdlog::warn("初始化svc_cache");
    }

    service_worker::service_worker(std::shared_ptr<memory> m, std::string host, std::string service, std::shared_ptr<svc_cache> endpoint_cache) : m_(m), host_(host), service_(service), endpoint_cache_(endpoint_cache)
    {
    }

    service_worker::~service_worker()
    {
        endpoint_cache_->return_back(host_, service_, m_);
    }

    awaitable<bool> service_worker::wait()
    {
        co_return co_await m_->wait();
    }

    awaitable<std::string_view> service_worker::async_load_some(std::size_t max_n)
    {
        co_return co_await m_->async_load_some(max_n);
    }

    awaitable<std::size_t> service_worker::async_write_some(std::string_view data)
    {
        co_return co_await m_->async_write_some(data);
    }

    awaitable<std::string_view> service_worker::async_load_until(const std::string &data)
    {
        co_return co_await m_->async_load_until(data);
    }

    awaitable<void> service_worker::async_write_all(std::string_view data)
    {
        co_return co_await m_->async_write_all(data);
    }

    std::string_view service_worker::get_some()
    {
        return m_->get_some();
    }

    std::size_t service_worker::remove_some(std::size_t index)
    {
        return m_->remove_some(index);
    }

    std::size_t service_worker::merge_some()
    {
        return m_->merge_some();
    }

    bool service_worker::ok()
    {
        return m_->ok();
    }

    void service_worker::reset()
    {
        // 没有此操作
        spdlog::error("unsupport");
    }

    awaitable<std::optional<msg_body>> response_headers::parser_headers(http_response &r)
    {
        auto sv = m_->get_some();
        r.response_header_str_ = sv.substr(0, header_end_ + 2);
        sv = sv.substr(0, header_end_);

        if (parser_header(sv, r.headers_))
        {
            m_->remove_some(header_end_ + 2);
            if (r.headers_["transfer-encoding"].find("chunked") != std::string_view::npos)
            {
                // https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1
                r.chunk_coding_ = true;
            }
            else if (auto s = msg_body_size(r.headers_); s)
            {
                r.body_size_ = *s;
            }
            else
            {
                // 没有msgbody
            }
            co_return std::make_optional<msg_body>();
        }

        co_return std::nullopt;
    }

    awaitable<std::optional<response_headers>> response_line::parser_response_line(http_response &r)
    {
        auto msg = co_await m_->async_load_until("\r\n\r\n");
        r.response_line_ = msg.substr(0, msg.find("\r\n") + 2);
        m_->remove_some(r.response_line_.size());
        co_return std::make_optional<response_headers>({msg.size() - r.response_line_.size() - 2, m_});
    }

    awaitable<bool> msg_body::parser_msg_body(http_response &)
    {
        co_return false;
    }

    awaitable<std::shared_ptr<memory>> socket_mem_factory::create(std::string host, std::string service)
    {
        // auto svc = host + ":" + service;
        // auto dns = slow_dns::get_slow_dns();
        // auto rrs = dns->resolve_cache({host, service});
        // if (!rrs)
        // {
        //     rrs.emplace(co_await dns->resolve({host, service}));
        // }

        // auto e = co_await this_coro::executor;
        // tcp_socket sock(e);
        // if (auto [error, remote_endpoint] = co_await asio::async_connect(sock, *rrs, asio::experimental::as_single(asio::use_awaitable), 0); error)
        // {
        //     spdlog::info("连接远程出错 -> {}", svc);
        //     hcpp::slow_dns::get_slow_dns()->remove_svc({host, service}, remote_endpoint.address().to_string());
        //     co_return std::shared_ptr<memory>{};
        // }
        auto sock = co_await make_socket(host, service);
        if (sock)
        {
            co_return std::make_shared<socket_memory>(std::move(*sock));
        }
        co_return std::shared_ptr<memory>{};
    }

} // namespace hcpp
