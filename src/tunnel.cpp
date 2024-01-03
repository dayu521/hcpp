#include "tunnel.h"

#include <spdlog/spdlog.h>

#include <asio/streambuf.hpp>
// #include <asio/read.hpp>
#include <asio/write.hpp>

#include <string>

awaitable<void> send_session(tunnel_session ts)
{
    auto & [client,server,ser_info]=ts;

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
        catch (std::exception &e)
        {
            spdlog::debug("client->async_read_some \n{}", e.what());
            break;
        }
        // spdlog::debug("请求数据\n{}", dv);
        try
        {
            co_await asio::async_write(*server, asio::buffer(data, n));
        }
        catch (std::exception &e)
        {
            spdlog::debug("async_write(*server \n{}", e.what());
            break;
        }
        total_n += n;
    }
    spdlog::debug("当前已写: {} kb -> 写 {} 关闭", total_n / 1024,ser_info);
}

awaitable<void> receive_session(tunnel_session ts)
{
    auto & [client,server,ser_info]=ts;

    char data[1024];
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
            spdlog::debug("server->async_read_some \n{}", e.what());
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
    }
    spdlog::debug("当前已读: {} kb -> 读 {} 关闭", total_n / 1024,ser_info);
}

awaitable<void> double_session(std::shared_ptr<tcp_socket> client, std::shared_ptr<tcp_socket> server)
{
    std::string data(1024, '\0');
    for (;;)
    {
        std::size_t n = 0;
        n = co_await client->async_read_some(asio::buffer(data, data.size()));
    }
    co_return;
}
