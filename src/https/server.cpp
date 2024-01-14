#include "server.h"

#include <thread>

#include <spdlog/spdlog.h>
#include <asio/buffer.hpp>

namespace hcpp
{
    awaitable<void> handle_tls_client(tls_client);

    awaitable<void> https_listen(std::shared_ptr<socket_channel> src)
    {
        // FIXME 这个需要用智能指针保证executor在https线程时是存在的吗?
        io_context executor;
        auto dd = [&](auto p)
        {
            spdlog::debug("收到主协程退出信号");
            executor.stop();
            delete p;
        };
        std::unique_ptr<int, decltype(dd)> gg(new int, dd);

        // 在当前协程运行
        auto https_listener = [&executor, src]() -> awaitable<void>
        {
            for (;;)
            {
                // 放到单独的协程运行
                auto client = co_await src->async_receive();
                co_spawn(executor, handle_tls_client(client), detached);
            }
        };

        auto https_service = [&executor]()
        {
            spdlog::debug("httpserver线程创建成功");
            while (!executor.stopped())
            {
                try
                {
                    auto work_guard = make_work_guard(executor);
                    executor.run();
                }
                catch (const std::exception &e)
                {
                    spdlog::error(e.what());
                }
            }
            spdlog::debug("httpserver线程退出成功");
        };
        std::thread t(https_service);
        t.detach();

        co_await https_listener();
        co_return;
    }

    using namespace asio::buffer_literals;
    awaitable<void> handle_tls_client(tls_client client)
    {
        spdlog::debug("执行https handle");
        auto executor = co_await this_coro::executor;
        ssl::context context(ssl::context::sslv23);
        context.set_options(
            asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::single_dh_use);
        // context.set_password_callback(std::bind(&server::get_password, this));
        // context.use_certificate_chain_file("rootCA.crt");
        // HACK 注意 一定要在之前调用
        context.set_password_callback([](auto a, auto b)
                                      { 
                                        spdlog::debug("执行 set_password_callback");
                                        return "123456"; });
        context.use_certificate_file("rootCA.crt", ssl::context::file_format::pem);
        context.use_private_key_file("rootCA.key", asio::ssl::context::pem);

        ssl::stream<tcp_socket> cli(tcp_socket(executor, ip::tcp::v4(), client.native_handle_), context);
        co_await cli.async_handshake(ssl::stream_base::server);
        std::string bf(1024, '\0');
        auto n = co_await cli.async_read_some(asio::buffer(bf, bf.size()));
        spdlog::debug("收到https消息\n{}", bf.substr(0, n));
        // HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 17\r\n\r\nHello,from https!
        co_await async_write(cli, buffer("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 17\r\n\r\nHello,from https!"_buf));
        co_return;
    }

} // namespace hcpp
