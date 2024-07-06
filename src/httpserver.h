#ifndef SRC_HTTPSERVER
#define SRC_HTTPSERVER

#include "https/mitm_svc.h"
#include "http/httpclient.h"
#include "http/tunnel.h"
#include "http/service_keeper.h"
#include "http/intercept.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/concurrent_channel.hpp>

#include <functional>
#include <memory>
#include <set>
#include <regex>
#include <unordered_map>
#include <optional>

namespace hcpp
{
    using asio::awaitable;
    using namespace asio::experimental;

    // using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    class http_handler
    {
    public:
        awaitable<void> http_do(std::unique_ptr<http_client> client, std::shared_ptr<service_keeper> sk);

        // 查找需要代理的主机
        std::optional<InterceptSet> find(std::string_view host);

        void add_request_handler(std::string_view path, std::function<std::string(std::string_view)> handler);

        void add_intercept(std::pair<std::string, InterceptSet>);
        void add_intercept(std::pair<std::regex, InterceptSet>);

    private:
        std::unordered_map<std::string_view, std::function<std::string(std::string_view)>> base_handlers_;

        std::unordered_map<std::string, InterceptSet> direct_filter_;
        std::vector<std::pair<std::regex, InterceptSet>> regx_filter_;
    };

    class hack_sk : public service_keeper
    {
    public:
        virtual awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service, std::shared_ptr<InterceptSet> is) override
        {
            co_return co_await sk_->wait(svc_host, svc_service, is);
        }

        virtual awaitable<std::shared_ptr<tunnel>> wait_tunnel(std::string svc_host, std::string svc_service, std::shared_ptr<InterceptSet> is) override
        {
            if (is && is->mitm_)
            {
                co_return std::make_shared<channel_tunnel>(socket_channel_,is);
            }
            co_return co_await sk_->wait_tunnel(svc_host, svc_service, is);
        }

    public:
        hack_sk(std::shared_ptr<service_keeper> sk, std::shared_ptr<socket_channel> socket_channel) : sk_(std::move(sk)), socket_channel_(socket_channel) {}
        std::shared_ptr<service_keeper> sk_;
        std::shared_ptr<socket_channel> socket_channel_;
    };

    class httpserver
    {
    protected:
        virtual std::shared_ptr<service_keeper> make_sk();

    public:
        awaitable<void> wait_http(uint16_t port);

        void set_http_handler(std::shared_ptr<http_handler> handler);
        void set_service_keeper(std::shared_ptr<service_keeper> sk);

    protected:
        std::shared_ptr<http_handler> handler_;
        std::shared_ptr<service_keeper> sk_;
    };

    struct subject_identify;

    class mimt_https_server : public httpserver
    {
    public:
        awaitable<void> wait_c();
        awaitable<void> wait(uint16_t port);

        void set_ch(std::shared_ptr<socket_channel> ch);

        void set_ca(subject_identify ca_subject);
        void set_root_verify_store_path(std::string_view root_verify_store_path);

    protected:
        std::shared_ptr<service_keeper> make_sk() override;

    private:
        std::shared_ptr<socket_channel> channel_;
        std::shared_ptr<subject_identify> ca_subject_;
        std::string root_verify_store_path_;
    };

    using notify_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::string)>>;

} // namespace hcpp

#endif /* SRC_HTTPSERVER */
