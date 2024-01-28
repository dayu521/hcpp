#include "httpserver.h"
#include "dns.h"
#include "http/httpclient.h"
#include "http/tunnel.h"
#include "http/http_svc_keeper.h"

#include <spdlog/spdlog.h>

namespace hcpp
{

    awaitable<void> http_do(http_client client, std::unique_ptr<service_keeper> sk)
    {
        while (true)
        {
            auto ss = client.get_memory();
            ss->reset();
            co_await ss->wait();

            auto req = client.make_request();
            auto p1 = req.get_first_parser(ss);

            if (auto p2 = co_await p1.parser_reuqest_line(req); p2)
            {
                if (auto p3 = co_await (*p2).parser_headers(req); p3)
                {
                    if (req.method_ == http_request::CONNECT)
                    {
                        auto t= co_await sk->wait_tunnel(req.host_,req.port_);
                        co_await t->make_tunnel(ss, req.host_, req.port_);
                        // co_await socket_tunnel::make_tunnel(ss, req.host_, req.port_, hcpp::slow_dns::get_slow_dns());
                        co_await ss->async_write_all("HTTP/1.1 200 OK\r\n\r\n");
                        spdlog::debug("建立http tunnel {}", req.host_);
                        co_return;
                    }
                    else if (!req.host_.empty())
                    {
                        req.headers_.erase("proxy-connection");
                        req.headers_["connection"] = "Connection: keep-alive\r\n";
                        std::string req_line = req.method_str_ + " " + req.url_ + " " + req.version_ + "\r\n";
                        for (auto &&header : req.headers_)
                        {
                            req_line += header.second;
                        }
                        req_line += "\r\n";

                        auto w = co_await sk->wait(req.host_, req.port_);

                        co_await w->async_write_all(req_line);
                        co_await req.transfer_msg_body(ss, w);
                        req.has_error_ = false;

                        http_response rp;
                        auto p1 = rp.get_first_parser(w);
                        if (auto p2 = co_await p1.parser_response_line(rp); p2)
                        {
                            if (auto p3 = co_await (*p2).parser_headers(rp); p3)
                            {
                                std::string msg_header = rp.response_line_ + rp.response_header_str_;
                                co_await ss->async_write_all(msg_header);
                                co_await rp.transfer_msg_body(w, ss);
                                continue;
                            }
                        }
                    }
                    else // TODO 用于控制自身行为
                    {
                        co_await ss->async_write_all("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 16\r\n\r\nHello,from http!");
                        continue;
                    }
                }
            }
            if (req.has_error_)
            {
                co_await ss->async_write_all("HTTP/1.1 400 Bad Request\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 16\r\n\r\nBad Request!");
            }
            else
            {
                co_await ss->async_write_all("HTTP/1.1 500 Error\r\n\r\n");
            }
        }
    }

    using tcp_acceptor = use_awaitable_t<>::as_default_on_t<ip::tcp::acceptor>;

    awaitable<void> httpserver::wait_http(uint16_t port)
    {
        auto executor = co_await this_coro::executor;

        tcp_acceptor acceptor(executor, {ip::tcp::v4(), port});
        spdlog::debug("http_server监听端口:{}", acceptor.local_endpoint().port());
        for (;;)
        {
            auto socket = co_await acceptor.async_accept();
            auto sk=std::make_unique<http_svc_keeper>(make_threadlocal_svc_cache(),slow_dns::get_slow_dns());
            co_spawn(executor, http_do(http_client(std::move(socket)), std::move(sk)), detached);
        }
    }

    void httpserver::attach_tunnel(tunnel_advice w)
    {
        // tunnel_advice_=w;
        // co_spawn(https_channel->get_executor(), mimt->wait_http(https_channel), detached);
    }

    awaitable<void> mimt_https_server::wait_http(std::shared_ptr<socket_channel> c)
    {
        // FIXME 这个需要用智能指针保证executor在https线程时是存在的吗?
        io_context executor;

        // 在当前协程运行
        auto https_listener = [&executor, c, this]() -> awaitable<void>
        {
            for (;;)
            {
                // 放到单独的协程运行
                //  auto client = co_await src->async_receive();
                // co_spawn(executor, http_service(http_client(std::move(socket)), std::make_shared<hcpp::socket_tunnel>()), detached);
            }
        };

        auto work_guard = make_work_guard(executor);

        auto https_service = [&executor]()
        {
            spdlog::debug("mimt https server线程创建成功");
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
            spdlog::debug("mimt https server线程退出成功");
        };
        std::thread t(https_service);
        t.detach();

        co_await https_listener();
        co_return;
    }

} // namespace hcpp