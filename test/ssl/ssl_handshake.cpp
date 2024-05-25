// BUG 复现三次校验证书

#include <asio/ssl.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/connect.hpp>
#include <asio/buffer.hpp>

#include <spdlog/spdlog.h>

#include <regex>
#include <optional>
#include <string>
#include <string_view>
#include <format>
#include <iostream>

using namespace asio;
using tcp_socket_co = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;
using tcp_resolver_co = asio::use_awaitable_t<>::as_default_on_t<ip::tcp::resolver>;
using ssl_stream_co = ssl::stream<tcp_socket_co>;

// namespace log = spdlog;

struct endpoint
{
    std::string host;
    std::string port;
};

inline std::optional<endpoint> check_http_url(std::string url)
{
    // thread_local static std::regex re{R"(^https?://([^/]+)/.*)"};
    thread_local static std::regex re{R"((https?)://([^/]+)(?::(\d+))?(/.*)?)"};
    std::smatch sm;
    endpoint ep{};
    if (std::regex_search(url, sm, re))
    {
        auto protocol = sm[1].str();
        ep.host = sm[2].str();
        if (sm[3].matched)
        {
            ep.port = sm[3].str();
        }
        else
        {
            if (protocol.size() == 5)
            {
                ep.port = "443";
            }
            else
            {
                ep.port = "80";
            }
        }
        return ep;
    }
    return std::nullopt;
}

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        spdlog::info("Usage: {} <http url>", argv[0]);
        return -1;
    }

    io_context io_context;

    auto task = [&io_context, url = argv[1]]() -> asio::awaitable<void>
    {
        auto ep = check_http_url(url);
        if (!ep)
        {
            spdlog::error("Invalid url: {}", url);
            co_return;
        }
        asio::ssl::context ctx{asio::ssl::context::sslv23};
        // ctx.set_verify_mode(ssl::verify_peer);
        // ctx.set_default_verify_paths();

        ssl_stream_co ssl_socket{io_context, ctx};
        ssl_socket.set_verify_mode(ssl::verify_peer);
        ssl_socket.set_verify_callback(
            [host = ep->host](auto ok, auto &v_ctx)
            {
                // auto res=ssl::host_name_verification(host)(ok, v_ctx);
                // spdlog::info("{} verify result: {}",host, res);
                // return res;
                spdlog::info("{} verify result: {}", host, true);
                return true;
            });
        
        auto &socket = ssl_socket.lowest_layer();

        // tcp_socket_co socket(io_context);

        tcp_resolver_co resolver(io_context);
        auto ip_list = co_await resolver.async_resolve(ep->host, ep->port);
        co_await async_connect(socket, ip_list, use_awaitable, 0);

        // ssl_stream_co ssl_socket{std::move(socket), ctx};

        co_await ssl_socket.async_handshake(ssl::stream_base::client);
        co_await async_write(ssl_socket, buffer(std::format("GET / HTTP/1.0\r\nHost: {}\r\n\r\n", ep->host)));

        char bf[512];
        while (true)
        {
            try
            {
                auto rn = co_await ssl_socket.async_read_some(buffer(bf));

                if (rn == 0)
                {
                    spdlog::info("断开连接 ");
                    break;
                }
                std::cout << std::string_view{bf, rn};
            }
            catch (std::exception &e)
            {
                spdlog::info("异常 {}", e.what());
                break;
            }
        }
        spdlog::info("退出");
    };
    co_spawn(io_context, task, detached);
    io_context.run();
    return 0;
}