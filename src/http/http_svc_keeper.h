#ifndef SRC_HTTP_HTTP_SVC_KEEPER
#define SRC_HTTP_HTTP_SVC_KEEPER
#include "memory.h"
#include "dns.h"
#include "httpmsg.h"
#include "socket_wrap.h"
#include "tunnel.h"

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <functional>

namespace hcpp
{
    struct response_headers
    {
        std::size_t header_end_;
        std::shared_ptr<memory> m_;
        awaitable<std::optional<msg_body>> parser_headers(http_response &);
    };

    struct response_line
    {
        std::shared_ptr<memory> m_;
        awaitable<std::optional<response_headers>> parser_response_line(http_response &);
    };

    struct http_response : http_msg
    {
        http_msg_line response_line_;

        std::string response_header_str_;

        response_line get_first_parser(std::shared_ptr<memory> m) { return {m}; }
    };

    class svc_cache;

    class http_svc_keeper;

    class service_worker : public memory
    {

    public:
        service_worker(std::shared_ptr<memory> m, std::string host, std::string service, std::shared_ptr<svc_cache> endpoint_cache);

        service_worker(const service_worker &) = delete;
        service_worker(service_worker &&) = default;
        service_worker &operator=(const service_worker &) = delete;

        ~service_worker();

    public:
        virtual awaitable<void> wait() override;

        virtual awaitable<std::string_view> async_load_some(std::size_t max_n) override;
        virtual awaitable<std::size_t> async_write_some(std::string_view) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(std::string_view) override;
        virtual std::string_view get_some() override;
        virtual std::size_t remove_some(std::size_t index) override;
        virtual bool ok() override;
        virtual void reset() override;

    private:
        std::shared_ptr<memory> m_;
        std::string host_;
        std::string service_;
        std::shared_ptr<svc_cache> endpoint_cache_;
    };

    class http_svc_keeper
    {

    public:
        http_svc_keeper(std::shared_ptr<svc_cache> cache, std::shared_ptr<slow_dns> dns);

        awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service);

        awaitable<std::shared_ptr<tunnel>> wait_tunnel(std::string svc_host, std::string svc_service);

    private:
        std::shared_ptr<slow_dns> slow_dns_;
        std::shared_ptr<svc_cache> endpoint_cache_;
    };

    class svc_cache
    {
    public:
        static std::shared_ptr<svc_cache> get_instance();

    public:
        awaitable<std::shared_ptr<memory>> get_endpoint(std::string host, std::string service, std::shared_ptr<slow_dns> dns);
        void return_back(std::string host, std::string service, std::shared_ptr<memory> m);
        bool remove_endpoint(std::string host, std::string service);

    private:
        svc_cache();

    private:
        struct imp;
        std::shared_ptr<imp> imp_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTP_SVC_KEEPER */
