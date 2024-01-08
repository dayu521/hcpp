#ifndef DNS_H
#define DNS_H

#include <vector>
#include <string>
#include <memory>
#include <utility>

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/any_io_executor.hpp>

namespace hcpp
{
    using resolver_results = asio::ip::basic_resolver_results<asio::ip::tcp>;
    using asio::awaitable;
    using asio::any_io_executor;

    struct host_mapping
    {
        std::string host_;
        std::vector<std::string> ips_;
    };

    struct dns_cfg
    {
        std::vector<host_mapping> mappings_;
    };

    class slow_dns
    {
    public:
        static std::shared_ptr<slow_dns> get_slow_dns();

    public:
        awaitable<resolver_results> resolve(std::string host, std::string service);
        std::optional<resolver_results> resolve_cache(const std::string &host);
        void remove_ip(const std::string &host, std::string_view ip);

        void init_resolver(any_io_executor executor);

    private:
        slow_dns();

        inline static thread_local std::unordered_map<std::string, asio::ip::basic_resolver_results<asio::ip::tcp>> local_dns;

        struct slow_dns_imp;
        std::shared_ptr<slow_dns_imp> imp_;
    };
} // namespace hcpp

#endif