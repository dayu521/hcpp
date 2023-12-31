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
// #include <asio/ip/basic_resolver.hpp>

// #include<asio.hpp>

#include <string>
#include <coroutine>
#include <thread>
#include <iostream>
#include <unordered_map>

#include <regex>

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

using headers = std::map<string, string>;

std::pair<bool, std::string_view> check_req_line_1(string_view svl, std::pair<string, string> &ts)
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
            ts.first = m[1].str();
            ts.second = m[2].str();
            // ts = m[0].str();
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

inline std::regex host_reg{R"(^([\w\.]+)(:(0|[1-9]\d{0,4}))?$)"};
inline std::regex valreg{R"(^[\w]$)"};

// https://www.rfc-editor.org/rfc/rfc2616#section-4.2
std::pair<bool, std::string_view> parser_header(string_view svl, headers &h)
{
    // Host: server.example.com
    bool ok = false;
    string_view error;
    while (svl.size() > 0)
    {
        auto name_end = svl.find_first_of(": ");
        if (name_end == string_view::npos)
        {
            goto L;
        }
        string name{svl.substr(0, name_end)};
        svl.remove_prefix(name_end + 2);

        // 找到所有值
        string val;
        auto val_end = svl.find_first_of("\r\n");
        if (name_end == string_view::npos)
        {
            goto L;
        }
        val += svl.substr(0, val_end);
        svl.remove_prefix(val_end + 2);
        // 如果可能的结束点跟随" ",则它不是真正的结束点
        while (svl.starts_with(" "))
        {
            val += " ";
            svl.remove_prefix(1);
            val_end = svl.find_first_of("\r\n");
            if (name_end == string_view::npos)
            {
                goto L;
            }
            val += svl.substr(0, val_end);
            svl.remove_prefix(val_end + 2);
        }
        // 我们认为val中的多值都会是空白分割的,注意,分割空白之后的空白都是空白的值

        if (auto [it, ok] = h.emplace(name, val); !ok)
        {
            h[name] += " ";
            h[name] += val;
        }
    }
T:
    return {true, error};
L:
    error = "HTTP/1.1 400 Bad Request";
    return {false, error};
}

awaitable<void> send_session(std::shared_ptr<tcp_socket> client, std::shared_ptr<tcp_socket> server)
{
    std::string data(1024, '\0');
    asio::streambuf line_buff;
    try
    {
        for (;;)
        {
            auto n = co_await client->async_read_some(asio::buffer(data, data.size()));
            // string str_line(n, '\0');
            // asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            // line_buff.consume(n);
            std::cout << data << std::endl;
            co_await server->async_send(asio::buffer(data, n));
        }
        co_return;
    }
    catch (std::exception &e)
    {
        std::cout << "send_session "<<e.what() << std::endl;
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
            auto n = co_await server->async_read_some(asio::buffer(data, data.size()));
            // string str_line(n, '\0');
            // asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            // line_buff.consume(n);
            std::cout << data << std::endl;
            co_await client->async_send(asio::buffer(data, n));
        }
        co_return;
    }
    catch (std::exception &e)
    {
        std::cout <<"receive_session "<< e.what() << std::endl;
    }
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
        string xx;
        std::pair<string, string> target_endpoint;
        for (;;)
        {

            // 这里不用 socket.async_read_some(asio::buffer(data));
            headers h;
            // 检查第一行
            auto n = co_await (asio::async_read_until(socket, line_buff, "\r\n"));

            // std::tuple<std::size_t, std::size_t> results =
            // co_await (
            //     asio::async_write(socket, asio::buffer(data)) && asio::async_read(socket, line_buff));

            std::istream is(&line_buff);

            string str_line(n, '\0');
            asio::buffer_copy(asio::buffer(str_line), line_buff.data(), n);
            line_buff.consume(n);
            std::cout << str_line << std::endl;

            if (auto [ok, error] = check_req_line_1(str_line, target_endpoint); !ok)
            {
                async_write(socket, asio::buffer(std::string(error) + "\r\n\r\n"));
                socket.close();
                co_return;
            }

            auto csmue_n = n;
            // 包含前面的行标记,这样才能与下面组成两个行标记
            string header_line{};
            if (auto left_n = line_buff.size(); left_n > 0)
            {
                header_line.resize(left_n);
                //
                asio::buffer_copy(asio::buffer(header_line), line_buff.data());
            }
            // 检查剩下行
            char str_some[256];

            while (!header_line.ends_with("\r\n\r\n"))
            {
                // 短路求值,减少比较次数
                if (header_line.size() <= 2 && header_line == "\r\n") // 没有其他请求了
                {
                    break;
                }
                auto nn = co_await socket.async_read_some(asio::buffer(str_some, sizeof(str_some)));
                header_line.append(str_some, nn);
                // str_some.clear();
            }

            std::cout << header_line << std::endl;
            if (auto [ok, error] = parser_header({header_line.begin(), header_line.begin() + header_line.size() - 2 /*去掉了结尾的CRLF*/}, h); !ok)
            {
                async_write(socket, asio::buffer(std::string(error) + "\r\n\r\n"));
                socket.close();
                co_return;
            }

            // 进行代理
            co_await async_write(socket, asio::buffer("HTTP/1.1 200 Connection Established\r\nProxy-agent: Node.js-Proxy\r\nConnection: keep-alive\r\n\r\n"));
            break;
        }
        // asio::io_context io_context(1);
        auto executor = co_await this_coro::executor;

        auto request_sock = std::make_shared<tcp_socket>(std::move(socket));
        auto respond_sock = std::make_shared<tcp_socket>(executor);

        tcp_resolver re{executor};
        auto [host, service] = target_endpoint;
        auto el = co_await re.async_resolve(host, service);
        auto xz = co_await asio::async_connect(*respond_sock, el);
        auto addr=xz.address().to_string();
        auto po=xz.port();
        std::cout<<addr<<" "<<po<<endl;
        // 请求协程
        co_spawn(executor, send_session(request_sock, respond_sock),detached);
        // 响应协程
        co_spawn(executor, receive_session(request_sock, respond_sock),detached);
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
