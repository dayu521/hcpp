#ifndef DNS_H
#define DNS_H

#include <vector>
#include <string>
#include <memory>

#include <asio/ip/tcp.hpp>

namespace hcpp
{
    using resolver_results =asio::ip::basic_resolver_results<asio::ip::tcp>;

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
        std::optional<resolver_results> resolve(std::string host, std::string service);

    private:
        slow_dns();

        struct slow_dns_imp;
        std::shared_ptr<slow_dns_imp> imp_;
    };
} // namespace hcpp

#endif