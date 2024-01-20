#include "tunnel.h"
#include "http_tunnel.h"

#include <string>

#include <spdlog/spdlog.h>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/write.hpp>

namespace hcpp
{
    /*
    awaitable<void> bind_write2(std::shared_ptr<memory> client, std::shared_ptr<memory> server, std::string server_info)
    {
        std::size_t total_n = 0;

        for (;;)
        {
            std::string_view data;
            std::size_t n = 0;
            try
            {
                data = co_await client->async_load_some(1024 * 4);
            }
            catch (asio::system_error &e)
            {
                spdlog::debug("client->async_read_some \n{}", e.what());
                break;
            }
            // spdlog::debug("请求数据\n{}", dv);

            try
            {
                co_await server->async_write_all(data);
            }
            catch (std::exception &e)
            {
                spdlog::debug("写异常 {} {}", server_info, e.what());
                break;
            }
            total_n += data.size();
            client->remove_some(data.size());
            if (n == 0)
            {
                spdlog::debug("客户端关闭连接");
                break;
            }
        }
        spdlog::debug("写 {} 关闭 -> {}kb", server_info, total_n / 1024);
    }

    awaitable<void> bind_read2(std::shared_ptr<memory> client, std::shared_ptr<memory> server, std::string server_info)
    {
        std::size_t total_n = 0;

        for (;;)
        {
            std::string_view data;
            std::size_t n = 0;
            try
            {
                data = co_await server->async_load_some(1024 * 4 * 4);
            }
            catch (std::exception &e)
            {
                spdlog::debug("读异常 {} {}", server_info, e.what());
                break;
            }

            try
            {
                co_await client->async_write_all(data);
            }
            catch (std::exception &e)
            {
                spdlog::debug("async_write(*client \n{}", e.what());
                break;
            }
            total_n += data.size();
            server->remove_some(data.size());
            if (n == 0)
            {
                spdlog::debug("{}关闭连接", server_info);
                break;
            }
        }
        spdlog::debug("读 {} 关闭 -> {}kb", server_info, total_n / 1024);
    }
*/
    awaitable<void> make_bind(http_client client, http_server server, std::string host, std::string service )
    {
        auto e = co_await this_coro::executor;
        auto c = client.get_socket();
        auto s = co_await server.get_socket(host, service);
        std::string_view msg = "HTTP/1.0 200 Connection established\r\n\r\n";
        co_await async_write(c,buffer(msg));
        co_spawn(e, bind_read({std::make_shared<tcp_socket>(std::move(c)), std::make_shared<tcp_socket>(std::move(s)), host+":"+service}), detached);
        co_spawn(e, bind_write({std::make_shared<tcp_socket>(std::move(c)), std::make_shared<tcp_socket>(std::move(s)), host+":"+service}), detached);

    }

} // namespace hcpp
