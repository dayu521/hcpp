#ifndef SRC_HTTP_HTTPSERVER
#define SRC_HTTP_HTTPSERVER
#include "memory.h"
#include "dns.h"

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <functional>

namespace hcpp
{

    struct http_response;

    struct msg_body2
    {
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_response &);
    };

    struct response_headers
    {
        awaitable<std::optional<msg_body2>> parser_headers(std::shared_ptr<memory> m, http_response &);
    };

    struct response_line
    {
        awaitable<std::optional<response_headers>> parser_response_line(std::shared_ptr<memory> m, http_response &);
    };

    struct http_response
    {
        using http_msg_line = std::string;
        using http_msg_body = std::vector<std::string>;
        using http_headers = std::unordered_map<std::string, http_msg_line>;

        http_headers headers_;
        http_msg_body bodys_;

        response_line get_first_parser();
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
        virtual awaitable<std::size_t> async_write_some(const std::string &) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(const std::string &) override;
        virtual std::string_view get_some() override;
        virtual void remove_some(std::size_t index) override;
        virtual bool eof() override;

    private:
        std::shared_ptr<memory> m_;
        std::string host_;
        std::string service_;
        std::shared_ptr<endpoint_cache> endpoint_cache_;
    };

    class http_server
    {

    public:
        http_server(std::shared_ptr<endpoint_cache> cache,std::shared_ptr<slow_dns> dns);

        awaitable<service_worker> wait(std::string svc_host, std::string svc_service);

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
        void return_back(std::string host,std::string service, std::shared_ptr<memory> m);
        bool remove_endpoint(const std::string &svc);

    // private:
        endpoint_cache();

    private:
        struct imp;
        std::shared_ptr<imp> imp_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTPSERVER */
