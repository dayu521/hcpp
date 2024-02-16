#include "httpserver.h"
#include "dns.h"
#include "http/httpclient.h"
#include "http/tunnel.h"
#include "http/http_svc_keeper.h"
#include "https/ssl_socket_wrap.h"
#include "config.h"

#include <limits>
#include <chrono>
#include <map>

#include <spdlog/spdlog.h>

namespace hcpp
{
    namespace log = spdlog;
    // inline std::latch latch(1);

    awaitable<void> http_do(std::unique_ptr<http_client> client, std::shared_ptr<service_keeper> sk)
    {
        client->init();
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
                        co_return;
                    }
                    else if (!req.host_.empty())
                    {
                        req.headers_.erase("proxy-connection");
                        // req.headers_["connection"] = "Connection: keep-alive\r\n";
                        std::string req_line = req.method_str_ + " " + req.url_ + " " + req.version_ + "\r\n";
                        for (auto &&header : req.headers_)
                        {
                            req_line += header.second;
                        }
                        req_line += "\r\n";

                        auto w = co_await sk->wait(req.host_, req.port_);

                        log::info("http_do:请求开始\n{}{}", req.host_, req.url_);
                        auto start_point = std::chrono::high_resolution_clock::now();
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
                                // 默认是持久连接
                                // https://www.rfc-editor.org/rfc/rfc2616#section-8.1
                                if (auto c = rp.headers_.find("connection"); c != rp.headers_.end())
                                {
                                    if (c->second.find("close") != std::string::npos)
                                    {
                                        log::warn("不保活 {}", req.host_);
                                        w->close();
                                    }
                                }

                                std::string msg_header = rp.response_line_ + rp.response_header_str_;
                                // log::error("{}响应头\n{}",req.host_,msg_header);
                                co_await ss->async_write_all(msg_header);
                                if (rp.chunk_coding_)
                                {
                                    log::debug("块传送开始 {}", req.host_);
                                    co_await rp.transfer_chunk(w, ss);
                                    log::debug("块传送结束 {}", req.host_);
                                }
                                else if (rp.body_size_ > 0)
                                {
                                    co_await rp.transfer_msg_body(w, ss);
                                }
                                else if (!w->alive())
                                {
                                    w->merge_some();
                                    auto str = w->get_some();
                                    if (!str.empty())
                                    {
                                        co_await ss->async_write_all(str);
                                        w->remove_some(str.size());
                                    }
                                    while (true)
                                    {
                                        str = co_await w->async_load_some();
                                        co_await ss->async_write_all(str);
                                        if (str.empty())
                                        {
                                            break;
                                        }
                                        w->remove_some(str.size());
                                    }
                                    // co_return;
                                }
                                else
                                {
                                    ;
                                }
                                auto end_point = std::chrono::high_resolution_clock::now();
                                auto du = std::chrono::duration_cast<std::chrono::milliseconds>(end_point - start_point).count();
                                log::info("http_do:请求结束\n{}{}\n间隔 {}ms ", req.host_, req.url_, du);
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
                auto hsk = std::make_shared<hack_sk>(std::move(sk), ta_);
                co_spawn(executor, http_do(std::make_unique<http_client>(std::move(socket)), std::move(hsk)), detached);
            }
            catch (const std::exception &e)
            {
                log::error("wait_http: {}", e.what());
            }
        }
    }

    void httpserver::attach_tunnel(tunnel_advice w)
    {
        ta_ = w;
    }

    static thread_local std::unordered_map<std::string, subject_identify> server_subject_map;

    awaitable<void> mimt_https_server::wait_c(std::size_t cn, std::vector<proxy_service> ps)
    {
        if (!ca_subject_)
        {
            throw std::runtime_error("ca_subject_ is null");
        }

        std::unordered_map<std::string, proxy_service> ps_map;
        for (auto &&i : ps)
        {
            auto k = i.host_;
            ps_map.emplace(k, std::move(i));
        }

        const decltype(ps_map) &cr_ps_map = ps_map;
        io_context executor;
        channel_ = std::make_shared<socket_channel>(co_await this_coro::executor, cn);

        co_await nc->async_send(asio::error_code{}, "ok");
        // 在当前协程运行
        auto https_listener = [&executor, c = channel_, cr_ps_map, ca_subject = ca_subject_]() -> awaitable<void>
        {
            for (;;)
            {
                try
                {
                    std::shared_ptr<channel_client> cc = co_await c->async_receive();
                    //BUG 这里以下部分需要放到协程执行,因为一些操作会阻塞住线程
                    auto hsc = std::make_unique<https_client>();
                    hsc->host_ = cc->host_;
                    hsc->service_ = cc->service_;
                    log::error("channel_client: {}", hsc->host_);
                    if (!cc->sock_)
                    {
                        log::error("无法获取socket");
                        continue;
                    }
                    // TODO 抽象工厂方法
                    auto sk = std::make_shared<mitm_svc>();

                    subject_identify si{};
                    if (auto i = cr_ps_map.find(hsc->host_); i != cr_ps_map.end())
                    {
                        sk->set_sni_host(i->second.sni_host_);
                        if (i->second.close_sni_)
                            sk->close_sni();
                    }

                    if (auto i = server_subject_map.find(hsc->host_); i != server_subject_map.end())
                    {
                        co_await sk->make_memory(hsc->host_, hsc->service_);
                        si = i->second;
                    }
                    else
                    {
                        part_cert_info pci{};
                        sk->add_SAN_collector(pci);
                        co_await sk->make_memory(hsc->host_, hsc->service_);
                        si = sk->make_fake_server_id(pci.dns_name_, ca_subject);
                        // XXX 插入失败不用管
                        server_subject_map.insert({hsc->host_, si});
                        log::error("mimt_https_server::wait_c:创建server_subject_map缓存 => {}", hsc->host_);
                        log::error("mimt_https_server::wait_c:当前缓存数量 {}", server_subject_map.size());
                    }

                    hsc->set_mem(co_await std::move(*cc).make(std::move(si)));

                    co_spawn(executor, http_do(std::move(hsc), std::move(sk)), detached);
                }
                catch (const std::exception &e)
                {
                    log::error("wait_c: {}", e.what());
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
        std::jthread t(https_service);

        std::unique_ptr<int, std::function<void(int *)>> ptr(new int(0), [&work_guard, &executor](auto &&p)
                                                             {
            log::info("work_guard分离");
            work_guard.reset();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            executor.stop();
            delete p; });

        co_await co_spawn(executor, https_listener(), use_awaitable);
        // co_await https_listener();
        co_return;
    }

    std::optional<std::shared_ptr<tunnel>> mimt_https_server::find_tunnel(std::string_view svc_host, std::string_view svc_service)
    {
        if (tunnel_set_.contains({svc_host.data(), svc_service.data()}))
        {
            return std::make_optional(std::make_shared<channel_tunnel>(channel_));
        }
        return std::nullopt;
    }

    void mimt_https_server::set_ca(subject_identify ca_subject)
    {
        ca_subject_ = std::make_shared<subject_identify>(std::move(ca_subject));
    }

    void mimt_https_server::set_root_verify_store_path(std::string_view root_verify_store_path)
    {
        root_verify_store_path_ = root_verify_store_path;
    }

} // namespace hcpp