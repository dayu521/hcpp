#include "server.h"

#include <thread>

#include <spdlog/spdlog.h>

namespace hcpp
{
    awaitable<void> handle_tls_client(tls_client);

    awaitable<void> https_listen(std::shared_ptr<socket_channel> src)
    {
        io_context executor;

        // 在当前协程运行
        auto https_listener = [&executor,src]() -> awaitable<void>
        {
            for (;;)
            {
                // 放到单独的协程运行
                auto client=co_await src->async_receive();
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
                    auto work_guard=make_work_guard(executor);
                    executor.run();
                }
                catch (const std::exception &e)
                {
                    spdlog::error(e.what());
                }
            }
            spdlog::debug("httpserver线程退出成功 是否停止{}",executor.stopped());
        };
        std::thread t(https_service);
        t.detach();

        // 退出,自动销毁executor
        co_await https_listener();
        exexutor.stop();
        spdlog::debug("协程销毁");
        co_return ;
    }

    awaitable<void> handle_tls_client(tls_client client)
    {
        ssl::context context(ssl::context::sslv23);
        context.set_options(
            asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2);
        // context.set_password_callback(std::bind(&server::get_password, this));
        // context.use_certificate_chain_file("rootCA.crt");
        context.use_certificate_file("rootCA.crt",ssl::context::file_format::pem);
        context.use_private_key_file("rootCA.key", asio::ssl::context::pem);
        context.set_password_callback([](auto a,auto b){
            return "123456";
        });
        auto executor=co_await this_coro::executor;
        ssl::stream<tcp_socket> cli(tcp_socket(executor,ip::tcp::v4(),client.native_handle_),context);
        co_await cli.async_handshake(ssl::stream_base::server);
        std::string bf(1024, '\0');
        auto n=co_await cli.async_read_some(asio::buffer(bf,bf.size()));
        spdlog::debug("收到https消息\n{}", bf.substr(0, n));
        co_await async_write(cli, asio::buffer("HTTP/1.1 200 OK\r\n\r\nHello, from https!"));
        co_return ;
    }

} // namespace hcpp
