#include "httpserver.h"
#include "dns.h"
#include "http/httpclient.h"
#include "http/tunnel.h"
#include "http/http_svc_keeper.h"

#include <spdlog/spdlog.h>

// #include <latch>
#include <set>

namespace hcpp
{
    namespace log = spdlog;
    // inline std::latch latch(1);

    awaitable<void> http_do(std::unique_ptr<http_client> client, std::unique_ptr<service_keeper> sk)
    {
        while (true)
        {
            auto ss = client->get_memory();
            ss->reset();
            if (!co_await ss->wait())
            {
                break;
            }

            auto req = client->make_request();
            auto p1 = req.get_first_parser(ss);

            if (auto p2 = co_await p1.parser_reuqest_line(req); p2)
            {
                if (auto p3 = co_await (*p2).parser_headers(req); p3)
                {
                    if (req.method_ == http_request::CONNECT)
                    {
                        auto t = co_await sk->wait_tunnel(req.host_, req.port_);
                        co_await t->make_tunnel(ss, req.host_, req.port_);
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
                        if (req.chunk_coding_)
                        {
                            co_await req.transfer_chunk(ss, w);
                        }
                        else if (req.body_size_ > 0)
                        {
                            co_await req.transfer_msg_body(ss, w);
                        }
                        else
                        {
                            // 没有体
                        }
                        req.has_error_ = false;

                        http_response rp;
                        auto p1 = rp.get_first_parser(w);
                        if (auto p2 = co_await p1.parser_response_line(rp); p2)
                        {
                            if (auto p3 = co_await (*p2).parser_headers(rp); p3)
                            {
                                if (rp.headers_["connection"].find("keep-alive") != std::string_view::npos)
                                {
                                    log::warn("保活 {}",req.host_);
                                    w->make_alive();
                                }
                                std::string msg_header = rp.response_line_ + rp.response_header_str_;
                                // log::error("{}响应头\n{}",req.host_,msg_header);
                                co_await ss->async_write_all(msg_header);
                                if (rp.chunk_coding_)
                                {
                                    log::debug("块传送开始 {}",req.host_);
                                    co_await rp.transfer_chunk(w, ss);
                                    log::debug("块传送结束 {}",req.host_);
                                }
                                else if (rp.body_size_ > 0)
                                {
                                    co_await rp.transfer_msg_body(w, ss);
                                }
                                else
                                {
                                    // 没有体
                                }
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
        co_await nc->async_receive();
        spdlog::debug("http_server监听端口:{}", acceptor.local_endpoint().port());
        for (;;)
        {
            try
            {
                auto socket = co_await acceptor.async_accept();
                auto sk = std::make_unique<http_svc_keeper>(make_threadlocal_svc_cache(), slow_dns::get_slow_dns());
                auto hsk = std::make_unique<hack_sk>(std::move(sk), ta_);
                co_spawn(executor, http_do(std::make_unique<http_client>(std::move(socket)), std::move(hsk)), detached);
            }
            catch (const std::exception &e)
            {
                log::error("wait_http: {}",e.what());
            }
        }
    }

    void httpserver::attach_tunnel(tunnel_advice w)
    {
        ta_ = w;
        // co_spawn(https_channel->get_executor(), mimt->wait_http(https_channel), detached);
    }

    awaitable<void> mimt_https_server::wait_c(std::size_t cn)
    {
        // FIXME 这个需要用智能指针保证executor在https线程时是存在的吗?
        io_context executor;
        channel_ = std::make_shared<socket_channel>(co_await this_coro::executor, cn);

        co_await nc->async_send(asio::error_code{}, "ok");
        // 在当前协程运行
        auto https_listener = [&executor, c = channel_]() -> awaitable<void>
        {
            for (;;)
            {
                try
                {
                    // 放到单独的协程运行
                    std::shared_ptr<channel_client> cc = co_await c->async_receive();
                    auto hsc = std::make_unique<https_client>();
                    hsc->host_ = cc->host_;
                    hsc->service_ = cc->service_;
                    log::info("channel_client: {}",hsc->host_);
                    if (!cc->sock_)
                    {
                        log::error("无法获取socket");
                        continue;
                    }
                    hsc->set_mem(co_await std::move(*cc).make());
                    auto sk = std::make_unique<mitm_svc>();
                    co_spawn(executor, http_do(std::move(hsc), std::move(sk)), detached);
                }
                catch (const std::exception &e)
                {
                    log::error("wait_c: {}",e.what());
                }
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

    // TODO 放配置文件里
    inline std::set<std::pair<std::string, std::string>> tunnel_set{
        {"github.com", "443"}, {"www.baidu.com", "443"},{"gitee.com", "443"}
        ,{"node4.pc", "8080"}
        };

    std::optional<std::shared_ptr<tunnel>> mimt_https_server::find_tunnel(std::string_view svc_host, std::string_view svc_service)
    {
        if (tunnel_set.contains({svc_host.data(), svc_service.data()}))
        {
            return std::make_optional(std::make_shared<channel_tunnel>(channel_));
        }
        return std::nullopt;
    }

} // namespace hcpp