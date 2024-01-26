#ifndef SRC_DNS
#define SRC_DNS

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

    using asio::any_io_executor;
    using asio::awaitable;

    struct host_mapping;
    struct dns_provider;

    class slow_dns
    {
    public:
        static std::shared_ptr<slow_dns> get_slow_dns();

    public:
        awaitable<edp_lists> resolve(host_edp hedp);
        std::optional<edp_lists> resolve_cache(host_edp hedp);
        void remove_svc(const host_edp & hedp, std::string_view ip);

        void load_hm(const std::vector<host_mapping> & hm);
        void load_dp(const std::vector<dns_provider> & dp);
        void save_hm(std::vector<host_mapping> & hm);

        // TODO 清理缓存.那些无法连接上的也需要清理

        // TODO 保存配置时,全写文件无意义.需要解析配置时得到修改处的位置

    private:
        slow_dns();

        inline static thread_local std::map<host_edp, edp_lists> local_dns;

        struct slow_dns_imp;
        std::shared_ptr<slow_dns_imp> imp_;
    };
} // namespace hcpp

#endif /* SRC_DNS */
