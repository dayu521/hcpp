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
#include <ranges>
#include <algorithm>

#include <spdlog/spdlog.h>

namespace hcpp
{
    namespace log = spdlog;
    // inline std::latch latch(1);

    awaitable<void> http_handler::http_do(std::unique_ptr<http_client> client, std::shared_ptr<service_keeper> sk)
    {
        std::string host = "";
        std::string svc = "";
        std::shared_ptr<InterceptSet> is = nullptr; // HACK  //TODO

        co_await client->init();

        while (true)
        {
            auto ss = client->get_memory();
            ss->reset();
            if (!ss->ok())
            {
                break;
            }
            co_await ss->wait();
            auto req = client->make_request();
            auto p1 = req.get_first_parser(ss);

            if (auto p2 = co_await p1.parser_reuqest_line(req); p2)
            {
                if (!(req.host_ == host && req.port_ == svc))
                {
                    if (auto opt = find(req.host_); opt)
                    {
                        is = std::make_shared<InterceptSet>(std::move(*opt));
                        if (!is->url_.empty())
                        {
                            // 替换主机
                            req.host_ = is->url_;
                            is->host_ = is->url_;
                        }
                    }else{
                        is = std::make_shared<InterceptSet>();
                    }
                    host = req.host_;
                    svc = req.port_;
                }

                if (auto p3 = co_await (*p2).parser_headers(req); p3)
                {
                    if (req.method_ == http_request::CONNECT)
                    {
                        auto t = co_await sk->wait_tunnel(req.host_, req.port_, is);
                        co_await t->make_tunnel(ss, req.host_, req.port_);
                        co_return;
                    }
                    else if (!req.host_.empty()) // BUG 普通http代理
                    {
                        req.headers_.erase("proxy-connection");
                        req.headers_.erase("host");
                        std::string req_line = req.method_str_ + " " + req.url_ + " " + req.version_ + "\r\n";
                        for (auto &&header : req.headers_)
                        {
                            req_line += header.second;
                        }
                        req_line+="Host: "+req.host_;
                        req_line += "\r\n\r\n";

                        auto w = co_await sk->wait(req.host_, req.port_, is);

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
                                        ss->close();
                                    }
                                }

                                std::string msg_header = rp.response_line_ + rp.response_header_str_;
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
                                    log::debug("httpdo: 没有响应体");
                                }
                                auto end_point = std::chrono::high_resolution_clock::now();
                                auto du = std::chrono::duration_cast<std::chrono::milliseconds>(end_point - start_point).count();
                                log::error("http_do:请求结束\n{}{} 间隔 {}ms ", req.host_, req.url_, du);
                                continue;
                            }
                        }
                    }
                    else // TODO 普通http请求.目前用于控制自身行为
                    {
                        if (auto p = base_handlers_.find({req.url_.data(), req.url_param_start_}); p != base_handlers_.end())
                        {
                            co_await ss->async_write_all((p->second)(req.url_));
                        }
                        else
                        {
                            co_await ss->async_write_all("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 16\r\n\r\nHello,from http!");
                            continue;
                        }
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

    void http_handler::add_intercept(std::pair<std::string, InterceptSet> i)
    {
        direct_filter_.emplace(std::move(i));
    }

    void http_handler::add_intercept(std::pair<std::regex, InterceptSet> i)
    {
        regx_filter_.emplace(regx_filter_.end(), std::move(i));
    }

    std::optional<InterceptSet> http_handler::find(std::string_view host)
    {
        std::optional<InterceptSet> r = std::nullopt;

        if (auto i = direct_filter_.find(host.data()); i != direct_filter_.end())
        {
            r = std::make_optional(i->second);
        }
        else if (auto i = std::ranges::find_if(regx_filter_, [host](auto &&i)
                                               { return std::regex_match(host.data(), i.first); });
                 i != regx_filter_.end())
        {
            r = std::make_optional(i->second);
        }

        return r;
    }

    void http_handler::add_request_handler(std::string_view path, std::function<std::string(std::string_view)> handler)
    {
        base_handlers_.insert({path, handler});
    }

    using tcp_acceptor = use_awaitable_t<>::as_default_on_t<ip::tcp::acceptor>;

    std::shared_ptr<service_keeper> httpserver::make_sk()
    {
        return std::make_shared<http_svc_keeper>(make_threadlocal_svc_cache(), slow_dns::get_slow_dns());
    }

    awaitable<void> httpserver::wait_http(uint16_t port)
    {
        auto executor = co_await this_coro::executor;

        tcp_acceptor acceptor(executor, {ip::tcp::v4(), port});
        spdlog::debug("http_server监听端口:{}", acceptor.local_endpoint().port());

        for (;;)
        {
            try
            {
                auto socket = co_await acceptor.async_accept();
                co_spawn(executor, handler_->http_do(std::make_unique<http_client>(std::move(socket)), make_sk()), detached);
            }
            catch (const std::exception &e)
            {
                log::error("wait_http: {}", e.what());
            }
        }
    }

    void httpserver::set_http_handler(std::shared_ptr<http_handler> handler)
    {
        handler_ = handler;
    }

    void httpserver::set_service_keeper(std::shared_ptr<service_keeper> sk)
    {
        sk_ = sk;
    }

    static thread_local std::unordered_map<std::string, subject_identify> server_subject_map;

    awaitable<void> mimt_https_server::wait_c()
    {
        if (!ca_subject_)
        {
            throw std::runtime_error("ca_subject_ is null");
        }

        io_context executor;

        // 在当前协程运行
        auto https_listener = [c = channel_, hander = handler_, ca_subject = ca_subject_](io_context &executor) -> awaitable<void>
        {
            http_handler hh;
            for (;;)
            {
                try
                {
                    std::shared_ptr<channel_client> cc = co_await c->async_receive();
                    // BUG 这里以下部分需要放到协程执行,因为一些操作会阻塞住线程
                    auto hsc = std::make_unique<https_client>();
                    hsc->host_ = cc->host_;
                    hsc->service_ = cc->service_;
                    if (!cc->sock_)
                    {
                        log::error("无法获取socket");
                        continue;
                    }
                    // TODO 抽象工厂方法
                    auto sk = std::make_shared<mitm_svc>();

                    hsc->init_ = [hander, sk, cc, ca_subject](https_client &self) -> awaitable<void>
                    {
                        auto is = cc->is_;
                        if (!is)
                        {
                            is = std::make_shared<InterceptSet>();
                        }
                        sk->set_sni_host(is->sni_host_);
                        if (is->close_sni_)
                            sk->close_sni();
                        subject_identify si{};

                        if (auto i = server_subject_map.find(self.host_); i != server_subject_map.end())
                        {
                            co_await sk->make_memory(self.host_, self.service_, is->doh_);
                            si = i->second;
                        }
                        else
                        {
                            part_cert_info pci{};
                            sk->add_SAN_collector(pci);
                            // 利用握手获取访问的主机的证书中的dns
                            co_await sk->make_memory(self.host_, self.service_, is->doh_);
                            // 创建假的证书
                            si = sk->make_fake_server_id(pci.dns_name_, ca_subject);
                            // XXX 插入失败不用管
                            server_subject_map.insert({self.host_, si});
                            log::error("mimt_https_server::wait_c:创建server_subject_map缓存 => {}", self.host_);
                            log::error("mimt_https_server::wait_c:当前缓存数量 {}", server_subject_map.size());
                        }

                        self.set_mem(co_await std::move(*cc).make(std::move(si)));
                    };

                    co_spawn(executor, hh.http_do(std::move(hsc), std::move(sk)), detached);
                }
                catch (const std::exception &e)
                {
                    log::error("wait_c: {}", e.what());
                }
            }
        };

        auto https_service = [&executor]()
        {
            spdlog::debug("mimt https server线程创建成功");
            auto work_guard = make_work_guard(executor);
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

        std::unique_ptr<int, std::function<void(int *)>> ptr(new int(0), [&executor](auto &&p)
                                                             {
            log::info("work_guard分离");
            executor.stop();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            delete p; });

        co_await https_listener(executor);
        // XXX 不会执行到这里
        co_return;
    }

    void mimt_https_server::set_ca(subject_identify ca_subject)
    {
        ca_subject_ = std::make_shared<subject_identify>(std::move(ca_subject));
    }

    void mimt_https_server::set_root_verify_store_path(std::string_view root_verify_store_path)
    {
        root_verify_store_path_ = root_verify_store_path;
    }

    std::shared_ptr<service_keeper> mimt_https_server::make_sk()
    {
        return std::make_shared<hack_sk>(httpserver::make_sk(), channel_);
    }

    void mimt_https_server::set_ch(std::shared_ptr<socket_channel> ch)
    {
        channel_ = ch;
    }

} // namespace hcpp