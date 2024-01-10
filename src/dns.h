#ifndef DNS_H
#define DNS_H

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <map>

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/any_io_executor.hpp>

namespace hcpp
{
    using edp_lists = std::vector<asio::ip::tcp::endpoint>;
    using host_edp = std::pair<std::string, std::string>;
    // using resolver_results = asio::ip::basic_resolver_results<asio::ip::tcp>;
    using asio::any_io_executor;
    using asio::awaitable;

    class slow_dns
    {
    public:
        static std::shared_ptr<slow_dns> get_slow_dns();

    public:
        awaitable<edp_lists> resolve(host_edp hedp);
        std::optional<edp_lists> resolve_cache(host_edp hedp);
        void remove_svc(const host_edp & hedp, std::string_view ip);

        void init_resolver(any_io_executor executor, std::string path = "");
        void save_mapping();

        // TODO 清理缓存.那些无法连接上的也需要清理

        // TODO 保存配置时,全写文件无意义.需要解析配置时得到修改处的位置

    private:
        slow_dns();

        inline static thread_local std::map<host_edp, edp_lists> local_dns;

        struct slow_dns_imp;
        std::shared_ptr<slow_dns_imp> imp_;
    };
} // namespace hcpp

#endif