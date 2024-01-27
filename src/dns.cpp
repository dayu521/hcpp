#include "dns.h"
#include "config.h"
#include "dns/dnshttps.h"
#include "dns/net-headers.h"
#include "https/ssl_socket_wrap.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <map>
#include <optional>
#include <iostream>
#include <algorithm>
#include <unordered_set>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/steady_timer.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>

// FIXME 注意,如果包含coro,编译失败,asio 1.28
#include <asio/experimental/concurrent_channel.hpp>

#include <spdlog/spdlog.h>

using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;
using tcp_socket = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::socket>;
using asio::detached;
using asio::use_awaitable;

using namespace asio::experimental::awaitable_operators;
using steady_timer_a = asio::use_awaitable_t<>::as_default_on_t<asio::steady_timer>;

using asio::experimental::concurrent_channel;
using channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, hcpp::edp_lists)>>;

namespace hcpp
{
    struct dns_cfg
    {
        std::vector<host_mapping> mappings_;
    };
    struct slow_dns::slow_dns_imp
    {
        std::vector<dns_provider> dns_providers_;

        std::shared_mutex smutex_;
        std::map<host_edp, edp_lists> edp_cache_;
        std::unique_ptr<tcp_resolver> resolver_;
        bool has_init_;
        bool has_save_;

        std::mutex rmutex_;
        using channel_await_list = std::vector<std::shared_ptr<channel>>;
        std::map<host_edp, channel_await_list> resolve_running_;

        std::string dns_path_;

        awaitable<edp_lists> resolve(host_edp hedp);

        awaitable<void> async_resolve(std::shared_ptr<channel> cc, host_edp hedp);

        void remove(host_edp hedp);
    };
    namespace
    {
        awaitable<edp_lists> resolve(host_edp hedp, const std::vector<dns_provider> & dns_providers)
        {
            using namespace harddns;
            auto c = co_await asio::this_coro::executor;
            asio::ssl::context ctx(asio::ssl::context::sslv23);

            auto svc_res = co_await tcp_resolver(c).async_resolve("", hedp.second);
            if (svc_res.empty())
            {
                throw std::runtime_error("empty svc");
            }

            auto svc = svc_res.begin()->endpoint().port();

            std::unordered_set<std::string> ips;
            for (auto &&provider : dns_providers)
            {
                tcp_resolver resolver(c);
                auto endpoints = resolver.resolve(provider.host_, "https");
                // auto endpoints = resolver.resolve("dns.alidns.com", "https");
                tcp_socket s(c);
                co_await asio::async_connect(s, endpoints);

                auto ssl_m=std::make_shared<ssl_sock_mem>(asio::ssl::stream_base::client);
                ssl_m->init(std::move(s));
                co_await ssl_m->wait(); 

                dnshttps dd(ssl_m, provider.host_);

                dnshttps::dns_reply dr;

                std::string res;
                co_await dd.get(hedp.first, htons(net_headers::dns_type::A), dr, res);
                for (auto &&i : dr)
                {
                    ips.insert(i.second.rdata);
                }
            }
            edp_lists el;
            for (auto &&i : ips)
            {
                asio::ip::tcp::endpoint edp(asio::ip::make_address(i), svc);
                el.push_back(edp);
            }
            co_return el;
        }
    }

    void slow_dns::slow_dns_imp::remove(host_edp hedp)
    {
        std::unique_lock<std::shared_mutex> l(smutex_);
        edp_cache_.erase(hedp);
        spdlog::info("dns缓存清除服务 {}", hedp.first);
    }

    std::shared_ptr<slow_dns> slow_dns::get_slow_dns()
    {
        static auto p = std::shared_ptr<slow_dns>(new slow_dns());
        return p;
    }

    awaitable<edp_lists> slow_dns::resolve(host_edp hedp)
    {
        if (auto r = resolve_cache(hedp); !r)
        {
            auto ip_edp = co_await imp_->resolve(hedp);
            local_dns.insert({hedp, ip_edp});

            for (auto &&i : ip_edp)
            {
                spdlog::debug("{} => {} {}", hedp.first, i.address().to_string(), i.port());
            }
            spdlog::debug("当前线程已缓存endpoint查询 {} 个", local_dns.size());

            co_return ip_edp;
        }
        else
        {
            co_return r.value();
        }
    }

    std::optional<edp_lists> slow_dns::resolve_cache(host_edp hedp)
    {

        if (auto ip_cache = local_dns.find(hedp); ip_cache != local_dns.end())
        {
            return {ip_cache->second};
        }
        else
        {
            return std::nullopt;
        }
    }

    void slow_dns::remove_svc(const host_edp &hedp, std::string_view ip)
    {
        local_dns.erase(hedp);
        imp_->remove(hedp);
        // TODO 全局缓存中对应条目,降低优先级或者直接取消这个ip
    }

    void slow_dns::load_hm(const std::vector<host_mapping> &hm)
    {
        if (imp_->dns_path_.empty())
        {
            return;
        }

        std::unique_lock<std::shared_mutex> m(imp_->smutex_);
        if (imp_->has_init_)
        {
            spdlog::error("slow_dns 检测到重复初始化");
            return;
        }

        auto tf = [](auto &&i)
        {
            edp_lists v;
            v.reserve(i.ips_.size());
            std::ranges::transform(i.ips_, std::back_inserter(v), [port = std::stoi(i.port_)](auto &&host)
                                   { return asio::ip::tcp::endpoint(asio::ip::make_address(host), port); });
            host_edp k = std::make_pair(i.host_, i.port_);
            return std::make_pair(k, v);
        };

        std::ranges::transform(hm, std::inserter(imp_->edp_cache_, imp_->edp_cache_.begin()), tf);
        imp_->has_init_ = true;
        spdlog::info("加载host mapping {} 个", hm.size());
    }

    void slow_dns::load_dp(const std::vector<dns_provider> &dp)
    {
        imp_->dns_providers_=dp;
    }

    void slow_dns::save_hm(std::vector<host_mapping> & hm)
    {
        if (imp_->dns_path_.empty())
        {
            return;
        }
        // TODO 可以直接复制一个容器再进行保存,减小加锁粒度
        std::unique_lock<std::shared_mutex> m(imp_->smutex_);
        if (imp_->has_save_)
        {
            spdlog::error("slow_dns 检测到重复保存");
            return;
        }
        hm.reserve(imp_->edp_cache_.size());

        std::ranges::transform(imp_->edp_cache_, std::back_inserter(hm), [](auto &&i)
                               {
            host_mapping hm;
            hm.host_=i.first.first;
            std::ranges::transform(i.second,std::back_inserter(hm.ips_),[](auto && i2){
                return i2.address().to_string();
            });
            if(hm.ips_.size()>0){
                hm.port_=std::to_string(i.second.begin()->port());
            }
            return hm; });

        imp_->has_save_ = true;
        spdlog::info("保存解析的host mapping {} 个", hm.size());
    }

    slow_dns::slow_dns() : imp_(std::make_shared<slow_dns_imp>())
    {
    }

    awaitable<edp_lists> slow_dns::slow_dns_imp::resolve(host_edp hedp)
    {
        {
            std::shared_lock<std::shared_mutex> m(smutex_);
            if (auto hm = edp_cache_.find(hedp); hm != edp_cache_.end())
            {
                co_return hm->second;
            }
        }

        auto ex = co_await asio::this_coro::executor;

        steady_timer_a t(ex);
        t.expires_after(std::chrono::seconds(8));

        // 注意,异步发送,一定要指定可发送的消息大小.
        auto cc = std::make_shared<channel>(ex, 1);

        // 有多个解析的主机名是不同的,他们应该不需要等待,直接解析
        // 单纯的使用基本的加锁只是解决线程的并发问题.需要采用高级协议,例如,消息队列模型
        //  这样,发送者可以对未解析完成的请求,及时超时返回失败.而消费者可以多个并发地进行解析
        //  解析时可以发现其他消费者是否已经解析,进而跳过不必要的解析.解析完成 发送消息唤醒

        // HACK 不直接使用channel https://github.com/chriskohlhoff/asio/issues/1175
        asio::co_spawn(ex, async_resolve(cc, hedp), detached);

        std::variant<edp_lists, std::monostate> results = co_await (cc->async_receive() || t.async_wait());
        if (results.index() != 0)
        {
            throw std::runtime_error("resolve host timeout");
        }
        co_return std::get<0>(results);
    }

    // FIXME 注意,如果使用asio::coro,这里就有线程问题,下面的加锁无效,仍旧出现死锁问题
    awaitable<void> slow_dns::slow_dns_imp::async_resolve(std::shared_ptr<channel> cc, host_edp hedp)
    {
        /// 协程内,协程切换时,一定不能持有锁,它都可能会和其他协程持有的锁互斥,除非所有都是共享锁

        // 查看是否有其他人正在解析当前host
        bool need_resolve = false;
        {
            std::unique_lock<std::mutex> m(rmutex_);
            if (auto querying = resolve_running_.find(hedp); querying != resolve_running_.end())
            {
                querying->second.push_back(cc);
            }
            else
            {
                resolve_running_.insert({hedp, {cc}});
                need_resolve = true;
            }
        }
        if (!need_resolve)
        {
            co_return;
        }

        edp_lists el;
        if (hedp.first == "github.com")
        {
            el = co_await hcpp::resolve(hedp, dns_providers_);
            // el.push_back({asio::ip::make_address("192.30.255.113"), 443});
        }
        else
        {
            tcp_resolver t(cc->get_executor());
            // 反而更慢,因为可能接收大量对同一个名字的解析
            // auto src = co_await t.async_resolve(hedp.first, hedp.second);
            auto src = t.resolve(hedp.first, hedp.second);

            el.reserve(src.size());
            std::ranges::transform(src, std::back_inserter(el), [](auto &&i)
                                   { return std::move(i.endpoint()); });
        }

        {
            std::unique_lock<std::shared_mutex> m(smutex_);
            if (auto res = edp_cache_.insert({hedp, el}); !res.second)
            {
                el = res.first->second;
            }
        }

        {
            std::unique_lock<std::mutex> m(rmutex_);
            auto channel_list = resolve_running_.find(hedp);

            auto sd = [&el](auto &&i)
            {
                // 失败的不管它
                i->try_send(asio::error_code{}, el);
            };
            std::ranges::for_each(channel_list->second, sd);
            resolve_running_.erase(hedp);
        }

        spdlog::info("解析远程端点:{} {}", hedp.first, hedp.second);
        for (auto &&ed : el)
        {
            spdlog::info("{}:{}", ed.address().to_string(), ed.port());
        }
        spdlog::info("共解析:{}个", edp_cache_.size());
    }

} // namespace hcpp
