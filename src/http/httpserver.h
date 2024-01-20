#ifndef SRC_HTTP_HTTPSERVER
#define SRC_HTTP_HTTPSERVER
#include "memory.h"
#include "dns.h"
#include "http.h"
#include "socket_wrap.h"

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <functional>

namespace hcpp
{
    struct response_headers
    {
        std::size_t header_end_;
        awaitable<std::optional<msg_body>> parser_headers(std::shared_ptr<memory> m, http_response &);
    };

    struct response_line
    {
        awaitable<std::optional<response_headers>> parser_response_line(std::shared_ptr<memory> m, http_response &);
    };

    struct http_response
    {
        http_msg_line response_line_;
        http_headers headers_;
        http_msg_body bodys_;

        response_line get_first_parser() { return {}; }
    };

    class endpoint_cache;

    class http_server;

    class service_worker : public memory
    {

    public:
        service_worker(std::shared_ptr<memory> m, std::string host, std::string service, std::shared_ptr<endpoint_cache> endpoint_cache);

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
        virtual void remove_some(std::size_t index) override;
        virtual bool ok() override;
        virtual void reset() override;

    private:
        std::shared_ptr<memory> m_;
        std::string host_;
        std::string service_;
        std::shared_ptr<endpoint_cache> endpoint_cache_;
    };

    class http_server
    {

    public:
        http_server(std::shared_ptr<endpoint_cache> cache, std::shared_ptr<slow_dns> dns);

        awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service);

        awaitable<tcp_socket> get_socket(std::string svc_host, std::string svc_service);

    private:
        std::shared_ptr<slow_dns> slow_dns_;
        std::shared_ptr<endpoint_cache> endpoint_cache_;
    };

    class endpoint_cache
    {
    public:
        static std::shared_ptr<endpoint_cache> get_instance();

    public:
        awaitable<std::shared_ptr<memory>> get_endpoint(std::string host, std::string service, std::shared_ptr<slow_dns> dns);
        void return_back(std::string host, std::string service, std::shared_ptr<memory> m);
        bool remove_endpoint(std::string host, std::string service);

        // private:
        endpoint_cache();

    private:
        struct imp;
        std::shared_ptr<imp> imp_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTPSERVER */
