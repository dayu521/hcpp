#ifndef SRC_HTTP_SERVER_KEEPER
#define SRC_HTTP_SERVER_KEEPER

#include "tunnel.h"
#include "hmemory.h"
#include "intercept.h"

#include <memory>
#include <string>

namespace hcpp
{
    class service_keeper
    {
    public:
        //普通http代理
        virtual awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service , std::shared_ptr<InterceptSet> is) = 0;

        //http隧道代理
        virtual awaitable<std::shared_ptr<tunnel>> wait_tunnel(std::string svc_host, std::string svc_service,std::shared_ptr<InterceptSet> is) = 0;

    public:
        virtual ~service_keeper() = default;
    };

    class sk_factory
    {
    public:
        virtual std::shared_ptr<service_keeper> create() = 0;
        virtual ~sk_factory() = default;
    };

} // namespace hcpp

#endif /* SRC_HTTP_SERVER_KEEPER */
