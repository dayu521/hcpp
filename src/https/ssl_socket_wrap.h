#ifndef SRC_HTTPS_SSL_SOCKET_WRAP
#define SRC_HTTPS_SSL_SOCKET_WRAP
#include "hmemory.h"
#include "asio_coroutine_net.h"

#include <set>

#include <asio/ssl.hpp>

namespace hcpp
{
    using namespace asio;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;
    using ssl_socket = ssl::stream<tcp_socket>;

    class ssl_socket_wrap : public memory
    {
    public:
        virtual awaitable<void> wait() override;
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n) override;
        virtual awaitable<std::size_t> async_write_some(std::string_view) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(std::string_view) override;
        virtual std::string_view get_some() override;
        virtual void remove_some(std::size_t) override;
        virtual void reset() override { read_index_ = write_index_ = 0; /*assert(buffs_.empty());*/ }

        struct mem_block
        {
            std::size_t begin_;
            std::string data_;
        };

    public:
        /// @brief
        /// @param sock 需要是准备读写状态
        ssl_socket_wrap(tcp_socket &&sock);
        /// @brief
        /// @param nh_sock_ 同样读写状态
        /// @param protocol_ 通过原始socket的endpoin获取protocol
        ssl_socket_wrap(tcp_socket::native_handle_type nh_sock_, tcp_socket::protocol_type protocol_);

    private:
        std::unique_ptr<ssl_socket> ssl_sock_;

        std::set<mem_block, std::less<>> buffs_;
        std::size_t read_index_ = 0;
        std::size_t write_index_ = 0;
    };

} // namespace hcpp

#endif /* SRC_HTTPS_SSL_SOCKET_WRAP */
