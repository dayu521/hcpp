#include "proxy.h"
#include "parser.h"
#include "http_tunnel.h"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/streambuf.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>

#include <spdlog/spdlog.h>

#include <unordered_map>
#include <regex>
#include <coroutine>

using asio::co_spawn;
using asio::detached;
using tcp_resolver = use_awaitable_t<>::as_default_on_t<tcp::resolver>;

awaitable<void> read_http_input(tcp_socket socket)
{
    using std::string,std::string_view;
    spdlog::debug("新连接 {}:{}", socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port());
    try
    {
        char data[1024];
        using spdlog::debug;

        std::unordered_map<string, std::shared_ptr<tcp_socket>> cache_sock;
        for (;;)
        {

            // 这里不用 socket.async_read_some(asio::buffer(data));
            debug("等待http请求...");
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
            debug("请求头 async_read_until \n{}", req_body_before);
            // std::cout << req_body_before << std::endl;

            // line_buff.consume(fle + 2);
            // 第一行结尾,不包含
            auto efl_1st = req_body_before.find("\r\n", 0) + 2;
            if (!check_req_line_1({req_body_before.data(), efl_1st}, target_endpoint, req_uri))
            {
                debug("检查请求行失败");
                co_await async_write(socket, asio::buffer(string_view("HTTP/1.1 400 Bad Request\r\n\r\n")));
                continue;
            }
            spdlog::debug("请求的远程服务 {}:{}", target_endpoint.host_, target_endpoint.port_);

            string_view req_header(req_body_before.begin() + efl_1st, req_body_before.begin() + n - 2 /*去掉了结尾的CRLF*/);
            spdlog::debug("请求头部分\n{}", req_header);
            // 所有请求头部分都读完了
            if (!parser_header(req_header, h))
            {
                debug("解析请求头失败");
                co_await async_write(socket, asio::buffer(string_view("HTTP/1.1 400 Bad Request\r\n\r\n")));
                continue;
            }

            const auto &[method, host, service] = target_endpoint;

            auto cache_sock_key = host + ":" + service;

            if (method == "CONNECT" || !cache_sock.contains(cache_sock_key)) // TODO 这里判断哪些连接被缓存,应该多个ip判断
            {
                // debug("开始与远程端点建立连接:{}",target_endpoint.host_);
                auto executor = socket.get_executor();

                tcp_resolver re{executor};

                auto el = co_await re.async_resolve(host, service);

                debug("解析远程端点:{}", host);

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
                debug("已与远程端点连接");
                debug("选择的 remote_endpoint-> {}:{}", redp.address().to_string(), redp.port());
                debug("选择的 local_endpoint-> {}:{}", led.address().to_string(), led.port());

                if (method == "CONNECT")
                {

                    auto request_sock = std::make_shared<tcp_socket>(std::move(socket));
                    co_spawn(executor, send_session({request_sock, respond_sock, host}), detached);
                    // 响应协程
                    co_spawn(executor, receive_session({request_sock, respond_sock, host}), detached);
                    debug("已建立http tunnel,当前协程退出,释放所有其他socket");

                    /// buffer传递原始字符串时一定要指定size大小,因为他会把普通字符串的最后的空字符也发送
                    // std::string r = "HTTP/1.0 200 Connection established\r\n\r\n";
                    co_await async_write(*request_sock, asio::buffer(std::string_view("HTTP/1.0 200 Connection established\r\n\r\n")));
                    co_return;
                }
                else
                {
                    debug("连接管理增加新的远程连接:{}", host);
                    cache_sock.insert({cache_sock_key, respond_sock});
                }
            }

            auto sock = cache_sock[cache_sock_key];
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
                debug("未实现 Chunked");
                co_return;
            }
            else if (h.contains("content-length"))
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
                auto nx = req_body_before.size() - n;
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
                    debug("Content-Length字段不合法 {}", cl);
                    co_return;
                }

                auto total_bytes = std::stoul(m[0].str());
                auto n = response_header.size() - n2;
                while (n < total_bytes)
                {
                    auto x = co_await sock->async_read_some(asio::buffer(data, sizeof(data)));
                    n += x;
                    co_await async_write(socket, asio::buffer(data, x));
                }
            }
            debug("http请求完成");
        }
    }
    catch (std::exception &e)
    {
        std::printf("echo Exception: %s\n", e.what());
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