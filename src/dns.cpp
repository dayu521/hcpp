#include "dns.h"

#include "json.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
// #include <unordered_set>
#include <optional>
#include <iostream>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/steady_timer.hpp>

// FIXME 注意,如果包含coro,编译失败,asio 1.28
#include <asio/experimental/concurrent_channel.hpp>
// #include <asio/experimental/channel.hpp>

#include <spdlog/spdlog.h>

using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;
using asio::detached;

using namespace asio::experimental::awaitable_operators;
using steady_timer = asio::use_awaitable_t<>::as_default_on_t<asio::steady_timer>;

// using asio::experimental::concurrent_channel;
using asio::experimental::concurrent_channel;
using channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, hcpp::resolver_results)>>;
// using channel = concurrent_channel<void(asio::error_code, hcpp::resolver_results)>;

namespace hcpp
{
    struct slow_dns::slow_dns_imp
    {
        std::shared_mutex smutex_;
        std::unordered_map<std::string, resolver_results> dns_cache_;
        std::unique_ptr<tcp_resolver> resolver_;

        std::mutex rmutex_;
        std::unordered_multimap<std::string, std::shared_ptr<channel>> resolve_running_;

        awaitable<resolver_results> resolve(std::string host, std::string service = "");

        awaitable<void> async_resolve(std::shared_ptr<channel> cc, std::string_view host, std::string_view service = "");
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

    void slow_dns::remove_ip(const std::string &host, std::string_view ip)
    {
        local_dns.erase(host);
        // TODO 全局缓存中对应条目,降低优先级或者直接取消这个ip
    }

    void slow_dns::init_resolver(any_io_executor executor)
    {
        // 初始化生成器
        imp_->resolver_ = std::make_unique<tcp_resolver>(executor);

        lsf::Json j;
        lsf::SerializeBuilder bu;

        auto res = j.run(std::make_unique<lsf::FileSource>("1.json"));
        if (!res)
        {
            std::cout << j.get_errors() << std::endl;
            throw std::runtime_error("解析出错");
        }
        lsf::json_to_string(*res, bu);
        
        spdlog::debug(bu.get_jsonstring());
        // std::cout << bu.get_jsonstring() << std::endl;

        // 解析
        std::unique_lock<std::shared_mutex> m(imp_->smutex_);
        // TODO 从配置文件中获取一些ip
    }

    void slow_dns::save_mapping()
    {
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

        auto ex = co_await asio::this_coro::executor;

        steady_timer t(ex);
        t.expires_after(std::chrono::seconds(3));

        auto cc = std::make_shared<channel>(ex, 0);

        // 有多个解析的主机名是不同的,他们应该不需要等待,直接解析
        // 单纯的使用基本的加锁只是解决线程的并发问题.需要采用高级协议,例如,消息队列模型
        //  这样,发送者可以对未解析完成的请求,及时超时返回失败.而消费者可以多个并发地进行解析
        //  解析时可以发现其他消费者是否已经解析,进而跳过不必要的解析.解析完成 发送消息唤醒
        asio::co_spawn(ex, async_resolve(cc, host, service), detached);

        std::variant<resolver_results, std::monostate> results = co_await (cc->async_receive() || t.async_wait());
        if (results.index() != 0)
        {
            throw std::runtime_error("resolve host timeout");
        }
        co_return std::get<0>(results);
    }

    // FIXME 注意,如果使用asio::coro,这里就有线程问题,下面的加锁无效,仍旧出现死锁问题
    awaitable<void> slow_dns::slow_dns_imp::async_resolve(std::shared_ptr<channel> cc, std::string_view host, std::string_view service)
    {
        /// 协程内,协程切换时,一定不能持有锁,它都可能会和其他协程持有的锁互斥,除非所有都是共享锁

        // 1. 查看是否有其他人正在解析当前host
        bool need_resolve = false;
        std::string h(host);
        {
            std::unique_lock<std::mutex> m(rmutex_);
            if (!resolve_running_.contains(h))
            {
                need_resolve = true;
            }
            resolve_running_.insert({h, cc});
        }
        if (!need_resolve)
        {
            co_return;
        }

        resolver_results el;
        tcp_resolver t(cc->get_executor());
        el = t.resolve(host, service);

        {
            std::unique_lock<std::shared_mutex> m(smutex_);

            if (!dns_cache_.contains(h))
            {
                dns_cache_.insert({h, el});
            }
        }

        {
            std::unique_lock<std::mutex> m(rmutex_);
            if (resolve_running_.contains(h))
            {
                for (auto &&it = resolve_running_.find(h); it != resolve_running_.end(); it++)
                {
                    // 失败的不管它
                    it->second->try_send(asio::error_code{}, el);
                }
            }
        }
        spdlog::info("解析远程端点:{}", host);
        for (auto &&ed : el)
        {
            spdlog::info("{}:{}", ed.endpoint().address().to_string(), ed.endpoint().port());
        }
        spdlog::info("已解析:{}个,返回当前解析", dns_cache_.size());
    }

} // namespace hcpp
