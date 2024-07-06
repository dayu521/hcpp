#ifndef SRC_HTTPS_MIMT_SVC
#define SRC_HTTPS_MIMT_SVC

#include "asio_coroutine_net.h"
#include "http/http_svc_keeper.h"
#include "hmemory.h"
#include "http/tunnel.h"
#include "http/thack.h"
#include "http/httpclient.h"
#include "http/intercept.h"

#include <string>
#include <vector>
#include <functional>

#include <asio/ssl.hpp>
#include <asio/experimental/concurrent_channel.hpp>

namespace hcpp
{
    using namespace asio;
    using namespace experimental;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

    struct channel_client
    {
        virtual ~channel_client() = default;
        virtual awaitable<std::shared_ptr<memory>> make(subject_identify si) &&;
        std::string host_;
        std::string service_;
        std::unique_ptr<tcp_socket> sock_;
        std::shared_ptr<InterceptSet> is_;
    };

    using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<channel_client>)>>;

    // HACK 好像只能支持发送存在默认构造函数的对象.所以不能使用std::unique_ptr
    // using socket_channel = asio::use_awaitable_t<>::as_default_on_t<concurrent_channel<void(asio::error_code, std::shared_ptr<tls_client>)>>;

    struct part_cert_info
    {
        std::vector<std::string> dns_name_;
        std::string pubkey_;
    };

    class ssl_mem_factory : public mem_factory
    {
    public:
        awaitable<std::shared_ptr<memory>> create(std::string host, std::string service) override;
    };

    template <typename T>
    class cache
    {
    public:
        virtual std::optional<T> get() {}
        virtual void put(T t) {}
    };

    // https://zh.cppreference.com/w/cpp/memory/enable_shared_from_this
    // XXX std::shared_ptr 的构造函数检测无歧义且可访问的 enable_shared_from_this 基类（即强制公开继承）
    class mitm_svc : public http_svc_keeper, public std::enable_shared_from_this<mitm_svc>
    {
    public:
        static std::unique_ptr<ssl_mem_factory> make_mem_factory()
        {
            return std::make_unique<ssl_mem_factory>();
        }
        awaitable<std::shared_ptr<memory>> wait(std::string svc_host, std::string svc_service,std::shared_ptr<InterceptSet> is) override;

    public:
        mitm_svc();
        void set_sni_host(std::string_view host);
        void close_sni();
        void add_SAN_collector(part_cert_info &pci);

        awaitable<void> make_memory(std::string svc_host, std::string svc_service,bool use_doh=false);

        subject_identify make_fake_server_id(const std::vector<std::string> &dns_name, std::shared_ptr<subject_identify> ca_si);

    private:
        std::unique_ptr<ssl_mem_factory> f_;
        std::shared_ptr<memory> m_;
        std::string sni_host_;
        bool enable_sni_ = true;

        std::function<bool(bool preverified, ssl::verify_context &v_ctx)> verify_fun_{};
    };

    struct https_client : public http_client
    {
        virtual http_request make_request() const override;

        virtual awaitable<void> init() override;

        void set_mem(std::shared_ptr<memory> m)
        {
            mem_ = m;
        }

    public:
        std::string host_;
        std::string service_;
        std::function<awaitable<void> (https_client & )> init_{}; 
    };

    class channel_tunnel : public tunnel, public std::enable_shared_from_this<channel_tunnel>, public mem_move
    {
    public:
        virtual awaitable<void> make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service) override;

    public:
        virtual void make(std::unique_ptr<tcp_socket> sock) override;
        virtual void make(std::unique_ptr<ssl_socket> sock) override;

    public:
        channel_tunnel(std::shared_ptr<socket_channel> channel,std::shared_ptr<InterceptSet> is) : channel_(channel), is_(is) {}

    private:
        std::shared_ptr<socket_channel> channel_;
        std::shared_ptr<channel_client> chc_;
        std::shared_ptr<InterceptSet> is_;
    };
} // namespace hcpp

#endif /* SRC_HTTPS_MIMT_SVC */
