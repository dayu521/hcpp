#ifndef SRC_HTTP_SOCKET_WRAP
#define SRC_HTTP_SOCKET_WRAP

#include "memory.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/streambuf.hpp>
#include <set>

namespace hcpp
{
    using namespace asio;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

    // XXX 线程不安全
    class socket_memory : public memory
    {
    public:
        virtual awaitable<void> wait() override;
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n) override;
        virtual awaitable<std::size_t> async_write_some(const std::string &) override;

        virtual awaitable<std::string_view> async_load_until(const std::string &) override;
        virtual awaitable<void> async_write_all(const std::string &) override;
        virtual std::string_view get_some() override;
        virtual void remove_some(std::size_t) override;
        virtual bool eof() override;

        struct mem_block
        {
            std::size_t begin_;
            std::string data_;
        };

    public:
        socket_memory(tcp_socket &&sock) : sock_(std::make_unique<tcp_socket>(std::move(sock))) {}

    private:
        std::unique_ptr<tcp_socket> sock_;

        std::set<mem_block, std::less<>> buffs_;
        std::size_t index_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_SOCKET_WRAP */
