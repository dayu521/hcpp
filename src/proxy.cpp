#include "proxy.h"
#include "parser.h"
#include "http_tunnel.h"
#include "dns.h"
#include "http/httpclient.h"
#include "http/tunnel.h"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <spdlog/spdlog.h>

#include <unordered_map>
#include <regex>

namespace hcpp
{

    using asio::co_spawn;
    using asio::detached;


    // TODO 根据规范里的顺序进行读取.其中content-length是在Chunked之后
    std::optional<std::size_t> msg_body_size(const http_headers &header)
    {
        if (header.contains("transfer-encoding"))
        {
            // TODO Chunked
            spdlog::error("未实现 Chunked");
            return std::nullopt;
        }
        else if (header.contains("content-length"))
        {
            string cl = header.at("content-length");
            static std::regex digit(R"(0|[1-9]\d{0,10})");
            std::smatch m;
            if (!std::regex_search(cl, m, digit))
            {
                spdlog::info("Content-Length字段不合法 {}", cl);
                return std::nullopt;
            }

            return std::stoul(m[0].str());
        }
        return std::nullopt;
    }

    awaitable<void> http_proxy(http_client client, std::shared_ptr<socket_channel> https_channel)
    {
        http_server server(hcpp::endpoint_cache::get_instance(), hcpp::slow_dns::get_slow_dns());
        while (true)
        {
            auto ss = client.get_memory();
            ss->reset();
            co_await ss->wait();
            http_request req;

            auto p1 = req.get_first_parser(ss);

            if (auto p2 = co_await p1.parser_reuqest_line( req); p2)
            {
                if (auto p3 = co_await (*p2).parser_headers( req); p3)
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

                        auto total_bytes = msg_body_size(req.headers_);
                        if (total_bytes)
                        {
                            co_await transfer_mem(ss, w, *total_bytes);
                        }

                        http_response rp;
                        auto p1 = rp.get_first_parser();
                        if (auto p2 = co_await p1.parser_response_line(w, rp); p2)
                        {
                            if (auto p3 = co_await (*p2).parser_headers(w, rp); p3)
                            {
                                std::string rp_line = rp.response_line_;
                                for (auto &&header : rp.headers_)
                                {
                                    rp_line += header.second;
                                }
                                rp_line += "\r\n";
                                co_await ss->async_write_all(rp_line);
                                auto total_bytes = msg_body_size(rp.headers_);
                                if (total_bytes)
                                {
                                    co_await transfer_mem(w, ss, *total_bytes);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace hcpp