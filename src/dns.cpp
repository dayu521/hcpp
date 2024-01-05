#include "dns.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <optional>

#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <spdlog/spdlog.h>

using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;
using asio::awaitable;
using asio::detached;

namespace hcpp
{
    struct slow_dns::slow_dns_imp
    {
        std::shared_mutex smutex_;
        std::unordered_map<std::string, resolver_results> dns_cache_;

        std::optional<resolver_results> resolve(std::string host, std::string service);

        awaitable<void> async_resolve(std::string_view host, std::string_view service);
    };

    std::shared_ptr<slow_dns> slow_dns::get_slow_dns()
    {
        static auto p = std::shared_ptr<slow_dns>(new slow_dns());
        return p;
    }

    std::optional<resolver_results> slow_dns::resolve(std::string host, std::string service)
    {
        static thread_local std::unordered_map<std::string, asio::ip::basic_resolver_results<asio::ip::tcp>> local_dns;

        if (auto ip_cache = local_dns.find(host); ip_cache == local_dns.end())
        {
            auto r = imp_->resolve(host, service);
            if (r)
            {
                local_dns.insert({host, r.value()});
                spdlog::debug("当前缓存dns查询 {}", local_dns.size());
            }
            return r;
        }
        else
        {
            return {ip_cache->second};
        }
    }

    slow_dns::slow_dns() : imp_(std::make_shared<slow_dns_imp>())
    {
    }

    std::optional<resolver_results> slow_dns::slow_dns_imp::resolve(std::string host, std::string service)
    {
        {
            {
                std::shared_lock<std::shared_mutex> m(smutex_);
                if (auto hm = dns_cache_.find(host); hm != dns_cache_.end())
                {
                    return hm->second;
                }
            }
            {

                co_spawn(executor, async_resolve(host, service), detached);
                std::shared_lock<std::shared_mutex> m(smutex_);
                if (auto hm = dns_cache_.find(host); hm != dns_cache_.end())
                {
                    return hm->second;
                }
            }
            return std::nullopt;
        }
    }

    awaitable<void> slow_dns::slow_dns_imp::async_resolve(std::string_view host, std::string_view service)
    {
        std::unique_lock<std::shared_mutex> m(smutex_);
        auto executor = co_await asio::this_coro::executor;
        static tcp_resolver r(executor);
        auto el = co_await r.async_resolve(host, service);
        spdlog::debug("解析远程端点:{}", host);

        for (auto &&ed : el)
        {
            spdlog::debug("{}:{}", ed.endpoint().address().to_string(), ed.endpoint().port());
        }
        dns_cache_.insert({std::string(host), el});
    }

} // namespace hcpp
