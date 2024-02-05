#ifndef SRC_HTTPS_SSL_SOCKET_WRAP
#define SRC_HTTPS_SSL_SOCKET_WRAP
#include "hmemory.h"
#include "asio_coroutine_net.h"

#include <set>

#include <asio/ssl.hpp>

namespace hcpp
{
    // using namespace asio;
    // using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;
    // using ssl_socket = ssl::stream<tcp_socket>;

    // XXX 线程不安全
    class ssl_sock_mem : public memory
    {
    public:
        virtual awaitable<bool> wait() override;
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n) override;
        virtual awaitable<std::size_t> async_write_some(std::string_view) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(std::string_view) override;
        virtual std::string_view get_some() override;
        virtual std::size_t remove_some(std::size_t) override;
        virtual std::size_t merge_some() override;
        virtual void reset() override;

        struct mem_block
        {
            std::size_t begin_;
            std::string data_;
        };

    public:
        void check(mem_move &) override;

    public:
        using ssl_stream_type = ssl::stream_base::handshake_type;

        /// @brief
        /// @param sock 需要是准备读写状态
        void init(tcp_socket &&sock);
        /// @brief
        /// @param nh_sock 同样读写状态
        /// @param protocol 通过原始socket的endpoin获取protocol
        awaitable<void> init(tcp_socket::native_handle_type nh_sock, tcp_socket::protocol_type protocol);

        awaitable<void> async_handshake();

        ssl_sock_mem(ssl_stream_type stream_type);

        void set_sni(std::string sni);
        void close_sni();

    private:
        ssl::stream_base::handshake_type stream_type_;
        std::unique_ptr<ssl::context> ctx_;
        //BUG https://github.com/chriskohlhoff/asio/issues/355
        //https://think-async.com/Asio/asio-1.28.0/doc/asio/reference/ssl__stream/_stream.html
        //A stream object must not be destroyed while there are pending asynchronous operations associated with it. 
        std::unique_ptr<ssl_socket> ssl_sock_;  

        std::set<mem_block, std::less<>> buffs_;
        std::size_t read_index_ = 0;
        std::size_t write_index_ = 0;
    };

} // namespace hcpp

#endif /* SRC_HTTPS_SSL_SOCKET_WRAP */
