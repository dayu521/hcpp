#include "proxy.h"
#include "parser.h"
#include "http_tunnel.h"
#include "dns.h"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/streambuf.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>
#include <asio/steady_timer.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/as_tuple.hpp>

#include <spdlog/spdlog.h>

#include <unordered_map>
#include <regex>
#include <coroutine>

using asio::co_spawn;
using asio::detached;
using tcp_resolver = use_awaitable_t<>::as_default_on_t<tcp::resolver>;
using steady_timer = use_awaitable_t<>::as_default_on_t<asio::steady_timer>;

using namespace asio::experimental::awaitable_operators;

using namespace asio::buffer_literals;

awaitable<std::size_t> transfer_message(tcp_socket &input, tcp_socket &output, std::size_t total_bytes)
{
    char data[1024 * 256];
    auto nx = 0;
    while (nx < total_bytes)
    {
        auto x = co_await input.async_read_some(asio::buffer(data));
        nx += x;
        co_await async_write(output, asio::buffer(data, x));
    }
    co_return nx;
}

awaitable<void> read_http_input(tcp_socket socket)
{
    using std::string, std::string_view;
    spdlog::debug("新连接 {}:{}", socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());
    try
    {
        char data[1024];
        using spdlog::debug;

        std::unordered_map<string, std::shared_ptr<tcp_socket>> keep_alive_sock;
        headers h;
        for (;;)
        {
            // 这里不用 socket.async_read_some(asio::buffer(data));
            debug("等待http请求...");
            RequestSvcInfo target_endpoint;
            asio::streambuf line_buff;
            string req_body_before{};
            string req_uri{};
            h.clear();
            size_t send_n = 0;
            size_t receive_n = 0;
            // 获取所有请求头部分
            steady_timer t(socket.get_executor());
            t.expires_after(std::chrono::seconds(10));
            std::variant<std::size_t, std::monostate> rt = co_await (asio::async_read_until(socket, line_buff, "\r\n\r\n") || t.async_wait());
            if (rt.index() != 0)
            {
                co_return;
            }
            auto n = std::get<0>(rt);
            send_n += n;

            // std::istream is(&line_buff);
            req_body_before.resize(line_buff.size());
            asio::buffer_copy(asio::buffer(req_body_before), line_buff.data());
            // std::cout << req_body_before << std::endl;

            // line_buff.consume(fle + 2);
            // 第一行结尾,不包含
            auto efl_1st = req_body_before.find("\r\n", 0) + 2;
            if (!check_req_line_1({req_body_before.data(), efl_1st}, target_endpoint, req_uri))
            {
                spdlog::info("检查请求行失败");
                co_await async_write(socket, asio::buffer("HTTP/1.1 400 Bad Request\r\n\r\n"_buf));
                continue;
            }
            debug("请求行\n{}", string_view({req_body_before.data(), efl_1st}));
            spdlog::debug("请求的远程服务 {}:{}", target_endpoint.host_, target_endpoint.port_);

            string_view req_header(req_body_before.begin() + efl_1st, req_body_before.begin() + n - 2 /*去掉了结尾的CRLF*/);
            spdlog::debug("请求头部分\n{}", req_header);
            // 所有请求头部分都读完了
            if (!parser_header(req_header, h))
            {
                spdlog::info("解析请求头失败");
                co_await async_write(socket, asio::buffer("HTTP/1.1 400 Bad Request\r\n\r\n"_buf));
                continue;
            }

            const auto &[method, host, service] = target_endpoint;

            auto remote_service = host + ":" + service;

            if (method == "CONNECT" || !keep_alive_sock.contains(remote_service)) // TODO 这里判断哪些连接被缓存,应该多个ip判断
            {
                // debug("开始与远程端点建立连接:{}",target_endpoint.host_);
                auto executor = socket.get_executor();

                auto rrs = hcpp::slow_dns::get_slow_dns()->resolve_cache(host);
                if (!rrs)
                {
                    rrs.emplace(co_await hcpp::slow_dns::get_slow_dns()->resolve(host, service));
                }

                auto rip = rrs.value();
                auto respond_sock = std::make_shared<tcp_socket>(executor);
                // 默认完成处理器包装成协程后,传递参数反而有重载歧义,不得不写个0解决,这里在asio库不同的版本,编译可能出错
                if (auto [error, remote_endpoint] = co_await asio::async_connect(*respond_sock, rip, asio::as_tuple(asio::use_awaitable), 0); error)
                {
                    // 如果重试,则客户端可能超时,即使解析成功,socket已经被关闭了
                    spdlog::info("连接远程出错 -> {} {}", host, remote_endpoint.address().to_string());
                    hcpp::slow_dns::get_slow_dns()->remove_ip(host,remote_endpoint.address().to_string());
                    co_return;
                }

                auto led = respond_sock->local_endpoint();
                auto redp = respond_sock->remote_endpoint();
                debug("已与远程端点连接");
                debug("{} -> {}:{}", remote_service, redp.address().to_string(), redp.port());
                debug("localhost -> {}:{}", led.address().to_string(), led.port());

                if (method == "CONNECT")
                {

                    auto request_sock = std::make_shared<tcp_socket>(std::move(socket));
                    co_spawn(executor, send_session({request_sock, respond_sock, host}), detached);
                    // 响应协程
                    co_spawn(executor, receive_session({request_sock, respond_sock, host}), detached);

                    /// buffer传递原始字符串时一定要指定size大小,因为他会把普通字符串的最后的空字符也发送
                    // std::string r = "HTTP/1.0 200 Connection established\r\n\r\n";
                    co_await async_write(*request_sock, asio::buffer("HTTP/1.0 200 Connection established\r\n\r\n"_buf));
                    debug("已建立http tunnel");
                    co_return;
                }
                else
                {
                    keep_alive_sock.insert({remote_service, respond_sock});
                    debug("连接管理增加新的远程连接:{}", host);
                }
            }

            auto sock = keep_alive_sock[remote_service];
            if (!sock->is_open())
            {
                spdlog::debug("sock未打开");
            }
            // 请求路径,代理头修改
            h.erase("proxy-connection");

            for (auto &&header : h)
            {
                req_uri += header.second;
            }
            req_uri += "\r\n";
            req_uri += req_body_before.substr(n);

            spdlog::debug("客户请求头\n{}", req_uri);
            co_await async_write(*sock, asio::buffer(req_uri));

            // Chunked Transfer Coding
            //   https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1
            //   https://www.rfc-editor.org/rfc/rfc2616#section-14.41
            //   Transfer-Encoding: chunked

            // TODO 根据规范里的顺序进行读取.其中content-length是在Chunked之后
            if (h.contains("transfer-encoding"))
            {
                // TODO Chunked
                spdlog::error("未实现 Chunked");
                co_return;
            }
            else if (h.contains("content-length"))
            {
                string cl = h["content-length"];
                static std::regex digit(R"(0|[1-9]\d{0,10})");
                std::smatch m;
                if (!std::regex_search(cl, m, digit))
                {
                    spdlog::info("Content-Length字段不合法 {}", cl);
                    co_return;
                }

                auto total_bytes = std::stoul(m[0].str());

                send_n += co_await transfer_message(socket, *sock, total_bytes - (req_body_before.size() - n));
            }
            // std::array<char, 65536> bf;
            // std::vector<char> bf2(8192);
            line_buff.consume(line_buff.size());
            // auto bbf=asio::buffer(bf);
            // co_await asio::async_read(*sock,asio::buffer(bf2));

            auto n2 = co_await asio::async_read_until(*sock, line_buff, "\r\n\r\n");
            receive_n += n2;
            string response_header(line_buff.size(), '\n');
            asio::buffer_copy(asio::buffer(response_header), line_buff.data());
            string_view rsv(response_header.begin(), response_header.begin() + n2);
            spdlog::debug("远程服务响应\n{}", rsv);

            auto rf = rsv.find("\r\n") + 2;
            rsv.remove_prefix(rf);
            rsv.remove_suffix(2);
            h.clear();

            if (!parser_header(rsv, h))
            {
                // 响应出错,直接断开连接
                co_return;
            }
            // 写响应头部分
            co_await asio::async_write(socket, asio::buffer(response_header));

            // 写响应体部分
            if (h.contains("content-length")) // TODO 或者其他不可知的长度时
            {
                string cl = h["content-length"];
                static std::regex digit(R"(0|[1-9]\d{0,10})");
                std::smatch m;
                if (!std::regex_search(cl, m, digit))
                {
                    spdlog::info("Content-Length字段不合法 {}", cl);
                    co_return;
                }

                auto total_bytes = std::stoul(m[0].str());

                receive_n += co_await transfer_message(*sock, socket, total_bytes - (response_header.size() - n2));
            }
            debug("http请求完成,客户端发送大小:{}kb,服务器返回大小:{}kb", send_n / 1024, receive_n / 1024);
        }
    }
    catch (std::exception &e)
    {
        spdlog::debug("Exception: {}", e.what());
    }
}

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