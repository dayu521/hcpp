#ifndef SRC_CONFIG
#define SRC_CONFIG

#include "json.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace hcpp
{
    // dns mapping config
    struct host_mapping
    {
        std::string host_;
        std::string port_;
        std::vector<std::string> ips_;
        JS_OBJECT(JS_MEMBER(host_), JS_MEMBER(port_), JS_MEMBER(ips_));
    };

    struct dns_provider
    {
        std::string provider_;
        std::string host_;
        JS_OBJECT(JS_MEMBER(provider_), JS_MEMBER(host_));
    };

    struct config_struct
    {
        //TODO 可以重载json解析 uint16_t
        int port_=55555;
        std::string ca_path_;
        std::string pkb_path_;
        std::string host_mapping_path_="";
        std::vector<dns_provider> dns_provider_={{.provider_="1.1.1.1", .host_="1.1.1.1"},{.provider_="ali", .host_="dns.alidns.com"}};
        
        JS_OBJECT(JS_MEMBER(port_),JS_MEMBER(ca_path_), JS_MEMBER(pkb_path_), JS_MEMBER(host_mapping_path_), JS_MEMBER(dns_provider_));
    };

    class slow_dns;

    class config
    {

    public:
        static constinit inline std::string CONFIG_DEFAULT_PATH = "./hcpp-cfg.json";
        static std::shared_ptr<config> get_config(std::string_view config_path = CONFIG_DEFAULT_PATH);

    public:
        config(const config &) = delete;
        ~config();

        bool load_host_mapping(std::shared_ptr<slow_dns> sd);
        bool load_dns_provider(std::shared_ptr<slow_dns> sd);
        uint16_t get_port()const;

        void save_callback(std::function<void(config &)> sc);

    public:
        std::vector<host_mapping> hm_;

    private:
        config(std::string_view config_path);

    private:
        config_struct cs_;
        std::vector<std::function<void(config &)>> save_callback_;
    };
} // namespace hcpp

#endif /* SRC_CONFIG */
