//
// echo_server_with_default.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// #define ASIO_SEPARATE_COMPILATION
// #include <asio/impl/src.hpp>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>
#include <asio/read.hpp>
#include <cstdio>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/connect.hpp>
#include <asio/use_awaitable.hpp>

#include <string>
#include <coroutine>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <array>

#include <spdlog/spdlog.h>

#include <regex>

#include "src/parser.h"

using std::string, std::string_view;

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable_t;
using asio::ip::tcp;
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;
using tcp_resolver = use_awaitable_t<>::as_default_on_t<tcp::resolver>;
namespace this_coro = asio::this_coro;

struct my_awaitable : std::suspend_always
{
    std::coroutine_handle<> *self_;
    constexpr void await_suspend(std::coroutine_handle<> h)
    {
        self_ = &h;
    }
};

struct my_task
{

    struct promise
    {
        std::coroutine_handle<> next_await_;
        my_awaitable get_return_object()
        {
            return {.self_ = &next_await_};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept
        {
            if (next_await_)
                next_await_.resume();
            return {};
        }
        void return_void() {}
        void unhandled_exception() {}
    };
};

my_task read_some(tcp_socket socket, std::string &buff, std::size_t len)
{
}

inline string match_str(string_view src, string_view p)
{
    return {};
}

inline std::regex endpoint_reg{R"(^([\w\.]+)(:(0|[1-9]\d{0,4}))?$)"};
inline std::regex valreg{R"(^[\w]$)"};

awaitable<void> send_session(std::shared_ptr<tcp_socket> client, std::shared_ptr<tcp_socket> server)
{
    // 不需要等待
    // auto xd = co_await asio::async_write(*server, asio::buffer(req_msg));

    std::string data(1024, '\0');
    asio::streambuf line_buff;
    try
    {
        for (;;)
        {
            // asio::error::eof
            std::size_t n = 0;
            try
            {
                n = co_await client->async_read_some(asio::buffer(data, data.size()));
            }
            catch (std::exception &e)
            {
                std::cout << "client->async_read_some " << e.what() << std::endl;
            }

            std::cout << "请求数据 " << data << std::endl;
            try
            {
                co_await asio::async_write(*server, asio::buffer(data, n));
            }
            catch (std::exception &e)
            {
                std::cout << "async_write(*server " << e.what() << std::endl;
            }

            // co_await server->async_send(asio::buffer(data, n));
        }
        co_return;
    }
    catch (std::exception &e)
    {
        std::cout << "send_session " << e.what() << std::endl;
    }
}

awaitable<void> receive_session(std::shared_ptr<tcp_socket> client, std::shared_ptr<tcp_socket> server)
{
    std::string data(1024, '\0');
    asio::streambuf line_buff;
    try
    {
        for (;;)
        {
            std::size_t n = 0;
            try
            {
                n = co_await server->async_read_some(asio::buffer(data, data.size()));
            }
            catch (std::exception &e)
            {
                std::cout << "server->async_read_some " << e.what() << std::endl;
            }

            std::cout << "响应数据 " << data << std::endl;
            try
            {
                co_await asio::async_write(*client, asio::buffer(data, n));
            }
            catch (std::exception &e)
            {
                std::cout << "async_write(*client " << e.what() << std::endl;
            }

            // auto n = co_await server->async_read_some(asio::buffer(data, data.size()));
            // // string str_line(n, '\0');
            // // asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            // // line_buff.consume(n);
            // std::cout << "服务数据 "<< data << std::endl;
            // co_await asio::async_write(*client,asio::buffer(data, n));
            // co_await client->a(asio::buffer(data, n));
        }
        co_return;
    }
    catch (std::exception &e)
    {
        std::cout << "receive_session " << e.what() << std::endl;
    }
}

awaitable<void>
echo(tcp_socket socket)
{
    spdlog::debug("新连接 {}:{}",socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());
    try
    {
        char data[1024];
        using spdlog::debug;
        using std::cout, std::endl;
        // std::set<string> target_endpoint_set;
        // string xx;

        std::map<string, std::shared_ptr<tcp_socket>> cache_sock;
        for (;;)
        {

            // 这里不用 socket.async_read_some(asio::buffer(data));

            RequestSvcInfo target_endpoint;
            asio::streambuf line_buff;
            string req_body_before{};
            string req_uri{};
            headers h;
            // 获取所有请求头部分
            auto n = co_await asio::async_read_until(socket, line_buff, "\r\n\r\n");

            // std::istream is(&line_buff);
            req_body_before.resize(line_buff.size());
            asio::buffer_copy(asio::buffer(req_body_before), line_buff.data());
            std::cout << req_body_before << std::endl;

            // line_buff.consume(fle + 2);
            // 第一行结尾,不包含
            auto efl_1st = req_body_before.find("\r\n",0) + 2;
            if (!check_req_line_1({req_body_before.data(), efl_1st}, target_endpoint, req_uri))
            {
                debug("检查请求行失败");
                co_await async_write(socket, asio::buffer("HTTP/1.1 400 Bad Request\r\n\r\n"));
                co_return;
            }

            // 包含前面的行标记,这样才能与下面组成两个行标记
            // string header_line{};
            // if (auto left_n = line_buff.size(); left_n > 0)
            // {
            //     header_line.resize(left_n);
            //     // asio::buffer_copy
            //     asio::buffer_copy(asio::buffer(header_line), line_buff.data() + req_line.size(), left_n);
            // }
            // // 检查剩下行
            // char str_some[256];

            // while (!header_line.ends_with("\r\n\r\n"))
            // {
            //     // 短路求值,减少比较次数
            //     [[unlikely]]if (header_line.size() <= 2 && header_line == "\r\n") // 没有其他请求了
            //     {
            //         break;
            //     }
            //     auto nn = co_await socket.async_read_some(asio::buffer(str_some, sizeof(str_some)));
            //     header_line.append(str_some, nn);
            //     // str_some.clear();
            // }
            string_view req_header(req_body_before.begin() + efl_1st, req_body_before.begin() + n - 2 /*去掉了结尾的CRLF*/);
            spdlog::debug("\n{}", req_header);
            // 所有请求头部分都读完了
            if (!parser_header(req_header, h))
            {
                debug("解析请求头失败");
                co_await async_write(socket, asio::buffer("HTTP/1.1 400 Bad Request\r\n\r\n"));
                co_return;
            }

            const auto &[method, host, service] = target_endpoint;
            spdlog::debug("{}:{}", host, service);

            auto cache_sock_key = host + ":" + service;

            if (target_endpoint.method_ == "CONNECT" || !cache_sock.contains(cache_sock_key)) // TODO 这里判断哪些连接被缓存,应该多个ip判断
            {
                auto executor = socket.get_executor();

                tcp_resolver re{executor};

                auto el = co_await re.async_resolve(host, service);

                if (spdlog::get_level() == spdlog::level::debug)
                {
                    for (auto &&ed : el)
                    {
                        spdlog::debug("{}:{}", ed.endpoint().address().to_string(), ed.endpoint().port());
                    }
                }
                auto respond_sock = std::make_shared<tcp_socket>(executor);
                auto conn_name = co_await asio::async_connect(*respond_sock, el);

                auto led = respond_sock->local_endpoint();
                auto redp = respond_sock->remote_endpoint();
                debug("remote_endpoint {}:{}", redp.address().to_string(), redp.port());
                debug("local_endpoint {}:{}", led.address().to_string(), led.port());

                if (target_endpoint.method_ == "CONNECT")
                {
                    co_await async_write(socket, asio::buffer("HTTP/1.1 200 Connection Established\r\nProxy-agent: Node.js-Proxy\r\nConnection: keep-alive\r\n\r\n"));
                    auto request_sock = std::make_shared<tcp_socket>(std::move(socket));
                    co_spawn(executor, send_session(request_sock, respond_sock), detached);
                    // 响应协程
                    co_spawn(executor, receive_session(request_sock, respond_sock), detached);
                    co_return;
                }
                else
                {
                    cache_sock.insert({cache_sock_key, respond_sock});
                }
            }

            auto sock = cache_sock[cache_sock_key];
            if (!sock->is_open())
            {
                spdlog::debug("sock未打开");
            }
            // 请求路径,代理头修改
            h.erase("Proxy-Connection");
            
            for (auto &&header : h)
            {
                req_uri += header.second;
            }
            req_uri += "\r\n";
            req_uri+=req_body_before.substr(n);

            spdlog::debug("客户请求头\n{}", req_uri);
            co_await async_write(*sock, asio::buffer(req_uri));
            if (h.contains(to_lower("Content-Length"))) // TODO 或者其他不可知的长度时
            {
                string cl = h[to_lower("Content-Length")];
                static std::regex digit(R"(0|[1-9]\d{0,10})");
                std::smatch m;
                if (!std::regex_search(cl, m, digit))
                {
                    debug("Content-Length字段不合法 {}", cl);
                    co_return;
                }

                auto total_bytes = std::stoul(m[0].str());
                auto nx = req_body_before.size()-n;
                while (nx < total_bytes)
                {
                    auto x = co_await socket.async_read_some(asio::buffer(data));
                    nx += x;
                    co_await async_write(*sock, asio::buffer(data, x));
                }
            }
            // std::array<char, 65536> bf;
            // std::vector<char> bf2(8192);
            line_buff.consume(line_buff.size());
            // auto bbf=asio::buffer(bf);
            // co_await asio::async_read(*sock,asio::buffer(bf2));

            auto n2 = co_await asio::async_read_until(*sock, line_buff, "\r\n\r\n");
            string response_header(line_buff.size(), '\n');
            asio::buffer_copy(asio::buffer(response_header), line_buff.data());
            string_view rsv(response_header.begin(),response_header.begin()+n2);
            auto rf=rsv.find("\r\n")+2;
            rsv.remove_prefix(rf);
            rsv.remove_suffix(2);
            h.clear();
            spdlog::debug("远程服务响应\n{}", rsv);
            if (!parser_header(rsv, h))
            {
                // 响应出错,直接断开连接
                co_return;
            }
            //写响应头部分
            co_await asio::async_write(socket, asio::buffer(response_header));

            //写响应体部分
            if (h.contains("content-length")) // TODO 或者其他不可知的长度时
            {
                string cl = h["content-length"];
                static std::regex digit(R"(0|[1-9]\d{0,10})");
                std::smatch m;
                if (!std::regex_search(cl, m, digit))
                {
                    debug("Content-Length字段不合法 {}", cl);
                    co_return;
                }

                auto total_bytes = std::stoul(m[0].str());
                auto n = response_header.size()-n2;
                while (n < total_bytes)
                {
                    auto x = co_await sock->async_read_some(asio::buffer(data,sizeof(data)));
                    n += x;
                    co_await async_write(socket, asio::buffer(data, x));
                }
            }

        }
    }
    catch (std::exception &e)
    {
        std::printf("echo Exception: %s\n", e.what());
    }
}

awaitable<void> listener()
{
    using std::cout, std::endl;
    // std::cout << std::this_thread::get_id() << std::endl;
    auto executor = co_await this_coro::executor;

    tcp_acceptor acceptor(executor, {tcp::v4(), 55555});
    auto d = acceptor.local_endpoint();
    cout << d.port() << endl;
    for (;;)
    {
        auto socket = co_await acceptor.async_accept();
        co_spawn(executor, echo(std::move(socket)), detached);
    }
}

int main()
{
    try
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("hello spdlog");
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           { io_context.stop(); });

        // std::cout << std::this_thread::get_id() << std::endl;
        co_spawn(io_context, listener(), [&io_context](std::exception_ptr eptr)
                 {
                     // try
                     // {
                     if (eptr)
                         std::rethrow_exception(eptr);
                     // }
                     // catch(const std::exception& e)
                     // {
                     //     std::cout << "Caught exception: '" << e.what() << "'\n";
                     //     io_context.stop();
                     // }
                 });

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::printf("Exception: %s\n", e.what());
    }
}
