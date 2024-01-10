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
    using resolver_results =std::vector<asio::ip::tcp::endpoint>;
    // using resolver_results = asio::ip::basic_resolver_results<asio::ip::tcp>;
    using asio::awaitable;
    using asio::any_io_executor;

    class slow_dns
    {
    public:
        static std::shared_ptr<slow_dns> get_slow_dns();

    public:
        awaitable<resolver_results> resolve(std::string host, std::string service);
        std::optional<resolver_results> resolve_cache(const std::string &host);
        void remove_ip(const std::string &host, std::string_view ip);

        void init_resolver(any_io_executor executor);
        void save_mapping();

        //TODO 清理缓存.那些无法连接上的也需要清理

        //TODO 保存配置时,全写文件无意义.需要解析配置时得到修改处的位置

    private:
        slow_dns();

        inline static thread_local std::unordered_map<std::string, resolver_results> local_dns;

        struct slow_dns_imp;
        std::shared_ptr<slow_dns_imp> imp_;
    };
} // namespace hcpp

#endif