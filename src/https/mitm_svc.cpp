#include "mitm_svc.h"
#include "http/tunnel.h"

#include <thread>

#include <spdlog/spdlog.h>
#include <asio/buffer.hpp>
#include <asio/bind_executor.hpp>
#include "mitm_svc.h"

namespace hcpp
{
    awaitable<void> handle_tls_client(std::shared_ptr<tls_client>);

    awaitable<void> https_listen(std::shared_ptr<socket_channel> src)
    {
        // FIXME 这个需要用智能指针保证executor在https线程时是存在的吗?
        io_context executor;

        // 在当前协程运行
        auto https_listener = [&executor, src]() -> awaitable<void>
        {
            for (;;)
            {
                // 放到单独的协程运行
                // auto client = co_await src->async_receive();
                // co_spawn(executor, handle_tls_client(co_await src->async_receive()), detached);
            }
        };

        auto work_guard = make_work_guard(executor);

        auto https_service = [&executor]()
        {
            spdlog::debug("httpserver线程创建成功");
            while (!executor.stopped())
            {
                try
                {           
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
    awaitable<void> handle_tls_client(std::shared_ptr<tls_client> client)
    {
        spdlog::debug("执行https handle");
        auto executor = co_await this_coro::executor;
        ssl::context context(ssl::context::sslv23);
        context.set_options(
            asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2);
        // HACK 注意 一定要在使用私钥之前调用
        context.set_password_callback([](auto a, auto b)
                                      { return "123456"; });
        // context.use_certificate_file("output.crt", ssl::context::file_format::pem);
        context.use_certificate_chain_file("server.crt.pem");
        context.use_private_key_file("server.key.pem", asio::ssl::context::pem);

        // bind_executor(executor,client->socket_);
        // if(client->socket_.get_executor()==executor){
        //     spdlog::debug("是在https中的执行器");
        // }
        //FIXME 不需要重新绑定socket到当前执行器吗?
        ssl::stream<tcp_socket> cli(std::move(client->socket_), context);
        // std::string bcnf(1024,'\0');
        co_await cli.async_handshake(ssl::stream_base::server);
        std::string bf(1024, '\0');
        auto n = co_await cli.async_read_some(asio::buffer(bf, bf.size()));
        spdlog::debug("收到https消息\n{}", bf.substr(0, n));
        co_await async_write(cli, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 17\r\n\r\nHello,from https!"_buf);
        co_return;
    }

    mitm_svc::mitm_svc():http_svc_keeper(make_threadlocal_svc_cache<mitm_svc>(),slow_dns::get_slow_dns())
    {
    }
    awaitable<std::shared_ptr<memory>> ssl_mem_factory::create(std::string host, std::string service)
    {
        return awaitable<std::shared_ptr<memory>>();
    }

} // namespace hcpp
