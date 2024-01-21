#ifndef SRC_HTTP_TUNNEL2
#define SRC_HTTP_TUNNEL2

#include "httpclient.h"
#include "httpserver.h"
#include "memory.h"
#include "dns.h"

#include <memory>

#include <asio/use_awaitable.hpp>

namespace hcpp
{
    class http_tunnel
    {
    public:
        http_tunnel(std::shared_ptr<memory> m, std::string host, std::string service);
        ~http_tunnel();

        awaitable<bool> wait(std::shared_ptr<slow_dns> dns);

        awaitable<void> read();
        awaitable<void> write();

        bool ok(){return read_ok_&&write_ok_;}

    public:
        static awaitable<void> bind_read(std::shared_ptr<http_tunnel> self);
        static awaitable<void> bind_write(std::shared_ptr<http_tunnel> self);
        static awaitable<void> make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service,std::shared_ptr<slow_dns> dns);

    private:


    private:
        std::shared_ptr<memory> c_;
        std::unique_ptr<memory> s_;
        std::string host_;
        std::string service_;
        std::size_t r_n_ = 0;
        std::size_t w_n_ = 0;

        bool read_ok_=true;
        bool write_ok_=true;
    };

} // namespace hcpp

#endif /* SRC_HTTP_TUNNEL */
