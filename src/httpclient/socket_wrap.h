#ifndef HTTP_CLIENT_SOCKET_WRAP_H
#define HTTP_CLIENT_SOCKET_WRAP_H

#include "clientbase.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/streambuf.hpp>

namespace hcpp
{
    using namespace asio;
    using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

    //XXX 线程不安全
    class socket_memory : public memory
    {
    public:
        virtual awaitable<std::size_t> async_read_some(std::string & buff,std::size_t max_n,std::size_t begin=0) override;
        virtual awaitable<std::size_t> async_write_some(const std::string &) override;

        virtual awaitable<std::pair<std::string,std::size_t>> async_read_until(const std::string &) override;
        virtual awaitable<void> async_write_all(const std::string &) override;
        virtual bool eof() override;

    private:
        std::unique_ptr<tcp_socket> sock_;
        asio::streambuf buff_;
    };

} // namespace hcpp

#endif