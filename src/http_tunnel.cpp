#include "http_tunnel.h"

#include <spdlog/spdlog.h>

#include <asio/streambuf.hpp>
// #include <asio/read.hpp>
#include <asio/write.hpp>

#include <string>

awaitable<void> send_session(tunnel_session ts)
{
    auto &[client, server, ser_info] = ts;

    char data[1024];
    std::size_t total_n = 0;

    for (;;)
    {
        // asio::error::eof
        std::size_t n = 0;
        try
        {
            n = co_await client->async_read_some(asio::buffer(data, sizeof(data)));
        }
        catch (asio::system_error &e)
        {
            spdlog::debug("client->async_read_some \n{}", e.what());
            server->shutdown(tcp_socket::shutdown_send);
            break;
        }
        // spdlog::debug("请求数据\n{}", dv);

        try
        {
            co_await asio::async_write(*server, asio::buffer(data, n));
        }
        catch (std::exception &e)
        {
            spdlog::debug("写异常 {} {}", ser_info, e.what());
            break;
        }
        total_n += n;
        if (n == 0)
        {
            spdlog::debug("客户端关闭连接");
            break;
        }
    }
    spdlog::debug("写 {} 关闭 -> {}kb", ser_info,total_n / 1024);
}

awaitable<void> receive_session(tunnel_session ts)
{
    auto &[client, server, ser_info] = ts;

    char data[1024 * 256];
    std::size_t total_n = 0;

    for (;;)
    {
        std::size_t n = 0;
        try
        {
            n = co_await server->async_read_some(asio::buffer(data, sizeof(data)));
        }
        catch (std::exception &e)
        {
            spdlog::debug("读异常 {} {}", ser_info, e.what());
            client->shutdown(tcp_socket::shutdown_send);
            break;
        }

        // spdlog::debug("响应数据\n{}", data);
        try
        {
            co_await asio::async_write(*client, asio::buffer(data, n));
        }
        catch (std::exception &e)
        {
            spdlog::debug("async_write(*client \n{}", e.what());
            break;
        }
        total_n += n;
        if (n == 0)
        {
            spdlog::debug("{}关闭连接",ser_info);
            break;
        }
    }
    spdlog::debug("读 {} 关闭 -> {}kb", ser_info,total_n / 1024);
}

awaitable<void> double_session(std::shared_ptr<tcp_socket> client, std::shared_ptr<tcp_socket> server)
{
    // std::string data(1024, '\0');
    // for (;;)
    // {
    //     std::size_t n = 0;
    //     n = co_await client->async_read_some(asio::buffer(data, data.size()));
    // }
    // co_return;
}
