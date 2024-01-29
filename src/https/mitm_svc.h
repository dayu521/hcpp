#ifndef SRC_HTTPS_MIMT_SVC
#define SRC_HTTPS_MIMT_SVC

#include "asio_coroutine_net.h"
#include "dns.h"
#include "http/http_svc_keeper.h"
#include "hmemory.h"
#include "http/tunnel.h"
#include "http/thack.h"
#include "http/httpclient.h"

#include <string>

#include <asio/ssl.hpp>
#include <asio/experimental/concurrent_channel.hpp>

namespace hcpp
{

    using namespace asio;
    using namespace experimental;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

    struct channel_client
    {
        virtual ~channel_client()=default;
        virtual awaitable<std::shared_ptr<memory>> make() &&;
        std::string host_;
        std::string service_;
        std::unique_ptr<tcp_socket> sock_;
    };

    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<channel_client>)>>;

    // HACK 好像只能支持发送存在默认构造函数的对象.所以不能使用std::unique_ptr
    // using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    struct https_client : public http_client
    {
        virtual http_request make_request() const override;

        void set_mem(std::shared_ptr<memory> m)
        {
            mem_ = m;
        }

    public:
        std::string host_;
        std::string service_;
    };

    awaitable<void>
    https_listen(std::shared_ptr<socket_channel> src);

    class ssl_mem_factory : public mem_factory
    {
    public:
        awaitable<std::shared_ptr<memory>> create(std::string host, std::string service) override;
    };

    class mitm_svc : public http_svc_keeper
    {
    public:
        static std::unique_ptr<ssl_mem_factory> make_mem_factory()
        {
            return std::make_unique<ssl_mem_factory>();
        }

    public:
        mitm_svc();
    };

    class channel_tunnel : public tunnel, public std::enable_shared_from_this<channel_tunnel>, public mem_move
    {
    public:
        virtual awaitable<void> make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service) override;

    public:
        virtual void make(std::unique_ptr<tcp_socket> sock) override;
        virtual void make(std::unique_ptr<ssl_socket> sock) override;

    public:
        channel_tunnel(std::shared_ptr<socket_channel> channel) : channel_(channel) {}

    private:
        std::shared_ptr<socket_channel> channel_;
        std::shared_ptr<channel_client> chc_;
    };
} // namespace hcpp

#endif /* SRC_HTTPS_MIMT_SVC */
