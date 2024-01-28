#ifndef SRC_HTTP_TUNNEL2
#define SRC_HTTP_TUNNEL2

#include "httpclient.h"
#include "http_svc_keeper.h"
#include "memory.h"
#include "dns.h"

#include <memory>

#include <asio/use_awaitable.hpp>

namespace hcpp
{
    class tunnel
    {
    public:
        virtual awaitable<void> make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service) = 0;
        virtual ~tunnel() = default;
    };

    class socket_tunnel : public tunnel, public std::enable_shared_from_this<socket_tunnel>
    {
    public:
        virtual awaitable<void> make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service);

    public:
        socket_tunnel();
        ~socket_tunnel();
        awaitable<void> read();
        awaitable<void> write();
        bool ok() { return c_->ok() && s_->ok(); }

    private:
        awaitable<bool> wait(std::shared_ptr<slow_dns> dns);

    private:
        std::shared_ptr<memory> c_;
        std::unique_ptr<memory> s_;
        std::string host_;
        std::string service_;
        std::size_t r_n_ = 0;
        std::size_t w_n_ = 0;
    };

} // namespace hcpp

#endif /* SRC_HTTP_TUNNEL */
