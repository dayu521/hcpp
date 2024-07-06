#ifndef SRC_HTTP_INTERCEPT
#define SRC_HTTP_INTERCEPT

#include <string>

namespace hcpp
{
    struct InterceptSet
    {
        std::string host_;  //请求的主机.标识key查询拦截
        std::string svc_; // 端口或服务名.例如 http https
        std::string url_;   //TODO 替换url
        std::string real_host_; //TODO 替换的主机名

        bool doh_ = false;  //使用doh查询主机ip
        std::string doh_provider_; //TODO 当前主机使用此提供商查询,而非全局配置的提供商

        bool mitm_ = false;
        bool close_sni_ = false; // 不发送sni信息.默认就是false.优先级高于sni_host_
        std::string sni_host_;   // 发送假的sni信息
    };
} // namespace hcpp

#endif /* SRC_HTTP_INTERCEPT */
