#include "dns.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <optional>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <spdlog/spdlog.h>

using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;
using asio::detached;

namespace hcpp
{
    struct slow_dns::slow_dns_imp
    {
        std::shared_mutex smutex_;
        std::unordered_map<std::string, resolver_results> dns_cache_;

        awaitable<resolver_results> resolve(std::string host, std::string service);

        awaitable<resolver_results> async_resolve(std::string_view host, std::string_view service);
    };

    std::shared_ptr<slow_dns> slow_dns::get_slow_dns()
    {
        static auto p = std::shared_ptr<slow_dns>(new slow_dns());
        return p;
    }

    // awaitable<std::optional<resolver_results>>

    // thread_local std::unordered_map<std::string, asio::ip::basic_resolver_results<asio::ip::tcp>> slow_dns::local_dns;

    awaitable<resolver_results> slow_dns::resolve(std::string host, std::string service)
    {
        if (auto r = resolve_cache(host); !r)
        {
            auto ip = co_await imp_->resolve(host, service);
            local_dns.insert({host, ip});
            spdlog::debug("当前缓存dns查询 {}", local_dns.size());

            co_return ip;
        }
        else
        {
            co_return r.value();
        }
    }

    std::optional<resolver_results> slow_dns::resolve_cache(const std::string &host)
    {

        if (auto ip_cache = local_dns.find(host); ip_cache != local_dns.end())
        {
            return {ip_cache->second};
        }
        else
        {
            return std::nullopt;
        }
    }

    void slow_dns::remove_ip(const std::string & host, std::string_view ip)
    {
        local_dns.erase(host);
        //TODO 全局缓存中对应条目,降低优先级或者直接取消这个ip
    }

    slow_dns::slow_dns() : imp_(std::make_shared<slow_dns_imp>())
    {
    }

    awaitable<resolver_results> slow_dns::slow_dns_imp::resolve(std::string host, std::string service)
    {

        {
            std::shared_lock<std::shared_mutex> m(smutex_);
            if (auto hm = dns_cache_.find(host); hm != dns_cache_.end())
            {
                co_return hm->second;
            }
        }

        co_return co_await async_resolve(host, service);
    }

    awaitable<resolver_results> slow_dns::slow_dns_imp::async_resolve(std::string_view host, std::string_view service)
    {
        /// 协程内,协程切换时,一定不能持有锁,它都可能会和其他协程持有的锁互斥,除非所有都是共享锁
        //  这里不能加锁
        tcp_resolver r(co_await asio::this_coro::executor);
        resolver_results el;
        {
            std::unique_lock<std::shared_mutex> m(smutex_);

            if (auto hm = dns_cache_.find(std::string(host)); hm != dns_cache_.end())
            {
                co_return hm->second;
            }

            el = r.resolve(host, service);

            dns_cache_.insert({std::string(host), el});
        }

        spdlog::info("解析远程端点:{}", host);
        for (auto &&ed : el)
        {
            spdlog::info("{}:{}", ed.endpoint().address().to_string(), ed.endpoint().port());
        }
        spdlog::info("已解析:{}个", dns_cache_.size());

        co_return el;
    }

} // namespace hcpp
