#ifndef SRC_HTTP_HTTP_SVC_KEEPER
#define SRC_HTTP_HTTP_SVC_KEEPER
#include "hmemory.h"
#include "dns.h"
#include "httpmsg.h"
#include "socket_wrap.h"
#include "tunnel.h"
#include "service_keeper.h"

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <functional>

#include <spdlog/spdlog.h>

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

    class service_worker : public memory
    {

    public:
        service_worker(std::shared_ptr<memory> m, std::string host, std::string service, std::shared_ptr<svc_cache> endpoint_cache);

        service_worker(const service_worker &) = delete;
        service_worker(service_worker &&) = default;
        service_worker &operator=(const service_worker &) = delete;

        ~service_worker();

    public:
        virtual awaitable<bool> wait() override;

        virtual awaitable<std::string_view> async_load_some(std::size_t max_n) override;
        virtual awaitable<std::size_t> async_write_some(std::string_view) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(std::string_view) override;
        virtual std::string_view get_some() override;
        virtual std::size_t remove_some(std::size_t index) override;
        virtual std::size_t merge_some() override;
        virtual bool ok() override;
        virtual void reset() override;
        virtual void close() override;
        virtual bool alive()override;

    private:
        std::shared_ptr<memory> m_;
        std::string host_;
        std::string service_;
        std::shared_ptr<svc_cache> endpoint_cache_;
    };

    class socket_mem_factory : public mem_factory
    {
    public:
        awaitable<std::shared_ptr<memory>> create(std::string host, std::string service) override;
    };

    class http_svc_keeper : public service_keeper
    {
    public:
        static std::unique_ptr<socket_mem_factory> make_mem_factory()
        {
            return std::make_unique<socket_mem_factory>();
        }

    public:
        http_svc_keeper(std::shared_ptr<svc_cache> cache, std::shared_ptr<slow_dns> dns);

        awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service) override;

        awaitable<std::shared_ptr<tunnel>> wait_tunnel(std::string svc_host, std::string svc_service) override;

    private:
        std::shared_ptr<slow_dns> slow_dns_;
        std::shared_ptr<svc_cache> endpoint_cache_;
    };

    class svc_cache
    {
    public:
        awaitable<std::shared_ptr<memory>> get_endpoint(std::string host, std::string service, std::shared_ptr<slow_dns> dns);
        void return_back(std::string host, std::string service, std::shared_ptr<memory> m);
        bool remove_endpoint(std::string host, std::string service);

        void set_mem_factory(std::unique_ptr<mem_factory> f);

    public:
        svc_cache();
        svc_cache(std::unique_ptr<mem_factory> mem_factory);

    private:
        struct imp;
        std::shared_ptr<imp> imp_;
    };

    template <typename T = http_svc_keeper>
    inline std::shared_ptr<svc_cache> make_threadlocal_svc_cache()
    {
        static thread_local auto p = std::shared_ptr<svc_cache>(new svc_cache(T::make_mem_factory()));
        return p;
    }

} // namespace hcpp

#endif /* SRC_HTTP_HTTP_SVC_KEEPER */
