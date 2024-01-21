#include "proxy.h"
#include "dns.h"
#include "http/httpclient.h"
#include "http/tunnel.h"

#include <spdlog/spdlog.h>

namespace hcpp
{

    awaitable<void> http_proxy(http_client client, std::shared_ptr<socket_channel> https_channel)
    {
        http_server server(endpoint_cache::get_instance(), slow_dns::get_slow_dns());
        while (true)
        {
            auto ss = client.get_memory();
            ss->reset();
            co_await ss->wait();

            http_request req;
            auto p1 = req.get_first_parser(ss);

            if (auto p2 = co_await p1.parser_reuqest_line(req); p2)
            {
                if (auto p3 = co_await (*p2).parser_headers(req); p3)
                {
                    if (req.method_ == http_request::CONNECT)
                    {
                        co_await http_tunnel::make_tunnel(ss, req.host_, req.port_, hcpp::slow_dns::get_slow_dns());
                        co_await ss->async_write_all("HTTP/1.1 200 OK\r\n\r\n");
                        spdlog::debug("已建立http tunnel");
                        co_return;
                    }
                    else
                    {
                        req.headers_.erase("proxy-connection");
                        std::string req_line = req.method_str_ + " " + req.url_ + " " + req.version_ + "\r\n";
                        for (auto &&header : req.headers_)
                        {
                            req_line += header.second;
                        }
                        req_line += "\r\n";

                        auto w = co_await server.wait(req.host_, req.port_);

                        co_await w->async_write_all(req_line);
                        co_await req.transfer_msg_body(ss,w);

                        http_response rp;
                        auto p1 = rp.get_first_parser(w);
                        if (auto p2 = co_await p1.parser_response_line(rp); p2)
                        {
                            if (auto p3 = co_await (*p2).parser_headers(rp); p3)
                            {
                                std::string msg_header=rp.response_line_+rp.response_header_str_;
                                co_await ss->async_write_all(msg_header);
                                co_await rp.transfer_msg_body(w,ss);
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace hcpp