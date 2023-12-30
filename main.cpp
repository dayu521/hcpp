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

#include <string>
#include <coroutine>
#include <thread>
#include <iostream>
#include <set>

#include <regex>

using std::string, std::string_view;

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable_t;
using asio::ip::tcp;
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;
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

// using target_server =std::set<string>;

std::pair<bool, std::string_view> check_req_line_1(string_view svl, string &ts)
{
    // string_view svl = str_line;
    string_view error = "";
    //"CONNECT * HTTP/1.1\r\n";
    {
        // 验证方法
        if (!svl.starts_with("CONNECT"))
        {
            error = "HTTP/1.1 405 Method Not Allowed";
            goto L;
            // co_await async_write(socket, asio::buffer("HTTP/1.1 405 Method Not Allowed"));
            // continue;
        }
        svl.remove_prefix(sizeof "CONNECT" - 1);
        if (svl.find_first_of(" ") == std::string_view::npos)
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        svl.remove_prefix(sizeof " " - 1);

        // 获取server name
        auto server_end = svl.find_first_of(" ");

        if (server_end == std::string_view::npos)
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        // auto server = svl.substr(0, server_end);

        //  (\w|\.)+(:(0|[1-9]\d{0,4}))? (\*|/?\w+(/\w+)*)
        std::regex host_reg(R"(([\w\.]+):(0|[1-9]\d{0,4}))");
        std::cmatch m;
        if (!std::regex_match(svl.begin(), svl.begin() + server_end, m, host_reg))
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        auto port = 0;
        for (int i = 0; auto &&sm : m)
        {
            std::cout << "    " << sm.str() << std::endl;
        }
        if (m.size() == 3)
        {
            port = std::stoi(m[2].str());
            if (port > 65535)
            {
                error = "HTTP/1.1 400 Bad Request";
                goto L;
            }
            // 插入失败说明存在了
            ts = m[0].str();
        }
        else
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }

        // 连同" "一起跳过
        assert(server_end + 1 <= svl.size());
        svl.remove_prefix(server_end + 1);

        // 检查http协议版本
        if (!svl.starts_with("HTTP/"))
        {
            error = "HTTP/1.1 405 Method Not Allowed";
            goto L;
        }
        svl.remove_prefix(sizeof "HTTP/" - 1);
        auto http_ver_end = svl.find_first_of("\r\n");

        if (http_ver_end == std::string_view::npos)
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        auto http_ver = svl.substr(0, http_ver_end);
        std::regex digit(R"(^\d{1,5}\.\d{1,10}$)");
        if (!std::regex_match(http_ver.begin(), http_ver.end(), digit))
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        assert(http_ver_end + 1 <= svl.size());
        svl.remove_prefix(http_ver_end + 2);
        assert(svl.size() == 0);
    }
L:
    if (error == "")
    {
        return {true, error};
    }
    else
    {
        return {false, error};
    }
}

std::pair<bool, std::string_view> check_req_line_2(string_view svl)
{
    // Host: server.example.com
    string_view error;
    {
        // HTTP/1.1 400 Bad Request
        if (!svl.starts_with("Host: "))
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
            // co_await async_write(socket, asio::buffer("HTTP/1.1 405 Method Not Allowed"));
            // continue;
        }
        svl.remove_prefix(sizeof "HOST: " - 1);

        auto host_end = svl.find_first_of("\r\n");
        if (host_end == std::string_view::npos)
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }

        std::regex host_reg(R"(^([\w\.]+)(:(0|[1-9]\d{0,4}))?$)");
        std::cmatch m;
        if (!std::regex_match(svl.begin(), svl.begin() + host_end, m, host_reg))
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
        for (int i = 0; auto &&sm : m)
        {
            std::cout << "    " << sm.str() << std::endl;
        }
        auto port = 0;
        if (m.size() == 4) // 全部匹配
        {
            port = std::stoi(m[3].str());
            if (port > 65535)
            {
                error = "HTTP/1.1 400 Bad Request";
                goto L;
            }
            svl.remove_prefix(host_end + 2);
            assert(svl.size() == 0);
        }
        else if (m.size() == 2)
        { // 没有端口
            svl.remove_prefix(host_end + 2);
            assert(svl.size() == 0);
        }
        else
        {
            error = "HTTP/1.1 400 Bad Request";
            goto L;
        }
    }
L:
    if (error == "")
    {
        return {true, error};
    }
    else
    {
        return {false, error};
    }
}

awaitable<void> proxy_session(string target, tcp_socket socket)
{
    std::string data;
    asio::streambuf line_buff;
    for (;;)
    {
        auto n = co_await socket.async_read_some(asio::buffer(data));
        // string str_line(n, '\0');
        // asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
        // line_buff.consume(n);
        std::cout << data << std::endl;
    }
    co_return;
}

awaitable<void>
echo(tcp_socket socket)
{
    std::cout << std::this_thread::get_id() << std::endl;
    try
    {
        char data[1024];
        asio::streambuf line_buff;
        using std::cout, std::endl;
        // std::set<string> target_endpoint_set;
        string target_endpoint;
        for (;;)
        {

            // 这里不用 socket.async_read_some(asio::buffer(data));

            // 检查第一行
            auto n = co_await asio::async_read_until(socket, line_buff, "\r\n");

            std::istream is(&line_buff);

            // string llll;
            // is>>llll;
            // cout<<llll<<endl;

            string str_line(n, '\0');
            asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            line_buff.consume(n);
            std::cout << str_line << std::endl;

            if (auto [ok, error] = check_req_line_1(str_line, target_endpoint); !ok)
            {
                co_await async_write(socket, asio::buffer(std::string(error) + "\r\n\r\n"));
                socket.close();
                co_return;
            }

            // 检查第二行
            n = co_await asio::async_read_until(socket, line_buff, "\r\n");

            str_line.resize(n);
            asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            line_buff.consume(n);
            std::cout << str_line << std::endl;
            if (auto [ok, error] = check_req_line_2(str_line); !ok)
            {
                co_await async_write(socket, asio::buffer(std::string(error) + "\r\n\r\n"));
                socket.close();
                co_return;
            }

            // 可不可以放到前面一块读了
            // n = co_await asio::async_read_until(socket, line_buff, "\r\n");
            // str_line.resize(n);
            // asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            // line_buff.consume(n);
            // std::cout << str_line << std::endl;
            // if (str_line != "\n\r")
            // {
            //     co_await async_write(socket, asio::buffer("HTTP/1.1 400 Bad Request\r\n\r\n"));
            //     socket.close();
            //     co_return;
            // }
            // 忽略剩下行

            // 进行代理
            co_await async_write(socket, asio::buffer("HTTP/1.1 200 ok\r\n\r\n"));
            break;
        }
        auto executor = co_await this_coro::executor;
        co_spawn(executor, proxy_session(target_endpoint, std::move(socket)), detached);
    }
    catch (std::exception &e)
    {
        std::printf("echo Exception: %s\n", e.what());
    }
}

awaitable<void> listener()
{
    using std::cout, std::endl;
    std::cout << std::this_thread::get_id() << std::endl;
    auto executor = co_await this_coro::executor;
    // asio::ip::tcp::endpoint endp{tcp::v4(), 55555};
    // tcp_acceptor acceptor(executor);
    // acceptor.open(endp.protocol());
    // acceptor.bind(endp);
    // acceptor.listen();
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
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           { io_context.stop(); });

        std::cout << std::this_thread::get_id() << std::endl;
        co_spawn(io_context, listener(), [&io_context](std::exception_ptr eptr)
                 { 
                    try
                    {
                        if (eptr)
                            std::rethrow_exception(eptr);
                    }
                    catch(const std::exception& e)
                    {
                        std::cout << "Caught exception: '" << e.what() << "'\n";
                        io_context.stop();
                    } });

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::printf("Exception: %s\n", e.what());
    }
}
